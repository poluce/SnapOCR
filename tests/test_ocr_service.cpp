#include <QtTest>

#include <memory>

#include <QElapsedTimer>
#include <QImage>
#include <QSignalSpy>
#include <QSize>

#include "ocr/FakeOcrEngine.h"
#include "ocr/OcrService.h"

using snapocr::ocr::FakeOcrEngine;
using snapocr::ocr::OcrLine;
using snapocr::ocr::OcrResult;
using snapocr::ocr::OcrService;

namespace {

QImage makeTestImage(int width = 320, int height = 200)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::white);
    return image;
}

OcrLine makeLine(const QString& text, const QRectF& boundingBox)
{
    OcrLine line;
    line.text = text;
    line.boundingBox = boundingBox;
    return line;
}

OcrResult resultAt(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).value<OcrResult>();
}

}  // namespace

class OcrServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void recognizesAsynchronously();
    void rejectsRequestWhileBusy();
    void returnsFailureWhenEngineMissing();
    void returnsFailureForEmptyImage();
    void propagatesEngineFailure();
    void timesOutAndDiscardsLateResult();
};

void OcrServiceTest::recognizesAsynchronously()
{
    OcrService service;
    auto fake = std::make_unique<FakeOcrEngine>();
    FakeOcrEngine* const fakePtr = fake.get();
    // 加入少量延迟，确保 recognizeAsync() 返回时仍处于忙碌态（避免调度竞态）。
    fakePtr->setDelayMs(30);

    OcrResult configured;
    configured.ok = true;
    configured.languageTag = QStringLiteral("fake-CN");
    configured.fullText = QStringLiteral("第一行\n第二行");
    configured.lines = {makeLine(QStringLiteral("第一行"), QRectF(10, 10, 100, 30)),
                        makeLine(QStringLiteral("第二行"), QRectF(10, 50, 100, 30))};
    fakePtr->setResult(configured);
    service.setEngine(std::move(fake));

    QSignalSpy spy(&service, &OcrService::finished);
    QVERIFY(spy.isValid());

    service.recognizeAsync(makeTestImage(), QStringLiteral("fake-CN"));
    QVERIFY(service.isBusy());
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    const OcrResult result = resultAt(spy, 0);
    QVERIFY(result.ok);
    QCOMPARE(result.fullText, QStringLiteral("第一行\n第二行"));
    QCOMPARE(result.languageTag, QStringLiteral("fake-CN"));
    QCOMPARE(static_cast<int>(result.lines.size()), 2);
    // 服务按端到端耗时填充 elapsedMs（>= 假引擎的 30ms 延迟）。
    QVERIFY(result.elapsedMs >= 20);
    QVERIFY(!service.isBusy());

    // 引擎在 worker 线程收到正确的图像尺寸与语言。
    QCOMPARE(fakePtr->recognizeCount(), 1);
    QCOMPARE(fakePtr->lastLanguageTag(), QStringLiteral("fake-CN"));
    QCOMPARE(fakePtr->lastImageSize(), QSize(320, 200));
}

void OcrServiceTest::rejectsRequestWhileBusy()
{
    OcrService service;
    auto fake = std::make_unique<FakeOcrEngine>();
    fake->setDelayMs(120);
    OcrResult configured;
    configured.ok = true;
    configured.fullText = QStringLiteral("完成");
    fake->setResult(configured);
    service.setEngine(std::move(fake));

    QSignalSpy spy(&service, &OcrService::finished);
    service.recognizeAsync(makeTestImage());
    QVERIFY(service.isBusy());
    service.recognizeAsync(makeTestImage());
    QVERIFY(service.isBusy());

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);

    int busyFailures = 0;
    int successes = 0;
    for (int index = 0; index < spy.count(); ++index)
    {
        const OcrResult result = resultAt(spy, index);
        if (result.ok)
        {
            ++successes;
        }
        else if (result.errorMessage.contains(QStringLiteral("正在识别中")))
        {
            ++busyFailures;
        }
    }
    QCOMPARE(successes, 1);
    QCOMPARE(busyFailures, 1);
    QVERIFY(!service.isBusy());
}

void OcrServiceTest::returnsFailureWhenEngineMissing()
{
    OcrService service;
    QSignalSpy spy(&service, &OcrService::finished);
    service.recognizeAsync(makeTestImage());
    QVERIFY(!service.isBusy());
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    const OcrResult result = resultAt(spy, 0);
    QVERIFY(!result.ok);
    QVERIFY(result.errorMessage.contains(QStringLiteral("引擎")));
}

void OcrServiceTest::returnsFailureForEmptyImage()
{
    OcrService service;
    auto fake = std::make_unique<FakeOcrEngine>();
    FakeOcrEngine* const fakePtr = fake.get();
    service.setEngine(std::move(fake));

    QSignalSpy spy(&service, &OcrService::finished);
    service.recognizeAsync(QImage());
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    const OcrResult result = resultAt(spy, 0);
    QVERIFY(!result.ok);
    QVERIFY(result.errorMessage.contains(QStringLiteral("图像")));
    // 空图像不应进入引擎。
    QCOMPARE(fakePtr->recognizeCount(), 0);
}

void OcrServiceTest::propagatesEngineFailure()
{
    OcrService service;
    auto fake = std::make_unique<FakeOcrEngine>();
    fake->setFailure(QStringLiteral("模拟识别失败"));
    service.setEngine(std::move(fake));

    QSignalSpy spy(&service, &OcrService::finished);
    service.recognizeAsync(makeTestImage());
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    const OcrResult result = resultAt(spy, 0);
    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("模拟识别失败"));
    QVERIFY(!service.isBusy());
}

void OcrServiceTest::timesOutAndDiscardsLateResult()
{
    OcrService service;
    service.setTimeoutMs(50);
    QCOMPARE(service.timeoutMs(), 50);

    auto fake = std::make_unique<FakeOcrEngine>();
    fake->setDelayMs(400);
    OcrResult configured;
    configured.ok = true;
    configured.fullText = QStringLiteral("迟到的结果");
    fake->setResult(configured);
    service.setEngine(std::move(fake));

    QSignalSpy spy(&service, &OcrService::finished);
    QElapsedTimer timer;
    timer.start();
    service.recognizeAsync(makeTestImage());
    QVERIFY(service.isBusy());
    QVERIFY(spy.wait(2000));

    // 超时结果应远早于 400ms 的引擎延迟到达。
    QVERIFY(timer.elapsed() < 300);
    QCOMPARE(spy.count(), 1);
    const OcrResult result = resultAt(spy, 0);
    QVERIFY(!result.ok);
    QVERIFY(result.errorMessage.contains(QStringLiteral("超时")));
    QVERIFY(!service.isBusy());

    // worker 仍在运行：其迟到结果必须被丢弃，不能再发 finished。
    QTest::qWait(600);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!service.isBusy());
}

QTEST_GUILESS_MAIN(OcrServiceTest)

#include "test_ocr_service.moc"
