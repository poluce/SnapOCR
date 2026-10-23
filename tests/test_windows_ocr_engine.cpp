#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QString>

#include "ocr/WindowsOcrEngine.h"

using namespace snapocr::ocr;

// 回归测试：Windows OCR 引擎在 UI（STA）线程被误调用时，必须返回中文错误而不是
// 触发 WinRT 的 `!is_sta_thread()` 断言导致进程崩溃（见 ocr-probe/probe.exe 事故）。
// 正常调用路径由 OcrService 保证在 worker 线程执行。
class WindowsOcrEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void recognizeOnUiThreadDoesNotCrash();
    void availableLanguagesDoesNotCrash();
    void recognizeNullImageReturnsError();
};

void WindowsOcrEngineTest::recognizeOnUiThreadDoesNotCrash()
{
    WindowsOcrEngine engine;
    QImage image(120, 60, QImage::Format_ARGB32);
    image.fill(Qt::white);

    QElapsedTimer timer;
    timer.start();
    const OcrResult result = engine.recognize(image, QStringLiteral("zh-Hans-CN"));
    const qint64 elapsed = timer.elapsed();

    // 不能崩溃、不能长时间阻塞。
    QVERIFY2(elapsed < 15000, qPrintable(QStringLiteral("elapsed=%1ms").arg(elapsed)));
    QVERIFY(!result.errorMessage.isNull());
    // 结果要么成功（主线程恰好是多线程公寓），要么是明确的中文错误。
    if (!result.ok) {
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

void WindowsOcrEngineTest::availableLanguagesDoesNotCrash()
{
    WindowsOcrEngine engine;
    const QVector<OcrLanguage> languages = engine.availableLanguages();
    Q_UNUSED(languages);
    QString reason;
    const bool available = engine.isAvailable(&reason);
    if (!available) {
        QVERIFY(!reason.isEmpty());
    }
}

void WindowsOcrEngineTest::recognizeNullImageReturnsError()
{
    WindowsOcrEngine engine;
    const OcrResult result = engine.recognize(QImage(), QString());
    QVERIFY(!result.ok);
    QVERIFY(!result.errorMessage.isEmpty());
}

QTEST_MAIN(WindowsOcrEngineTest)

#include "test_windows_ocr_engine.moc"
