#include <QtTest>

#include <QGuiApplication>

#include "platform/win32/CrashReporter.h"
#include "platform/win32/ProcessMetrics.h"

using namespace snapocr::platform::win32;

// 稳定性审计基础（阶段三 §8.2）：资源采样可用、异常码翻译正确。
class ProcessMetricsTest final : public QObject
{
    Q_OBJECT

private slots:
    void samplesRealMetrics();
    void describeContainsAllFields();
    void exceptionCodesAreTranslated();
};

void ProcessMetricsTest::samplesRealMetrics()
{
    ProcessMetrics metrics;
    QVERIFY(sampleProcessMetrics(&metrics));
    QVERIFY(metrics.handleCount > 0);
    // offscreen 平台不创建真实窗口，GDI 对象可能为 0；真实 windows 平台必须 > 0
    if (QGuiApplication::platformName() == QLatin1String("windows")) {
        QVERIFY(metrics.gdiObjects > 0);
    }
    QVERIFY(metrics.workingSetBytes > 0);
    QVERIFY(metrics.peakWorkingSetBytes >= metrics.workingSetBytes);
}

void ProcessMetricsTest::describeContainsAllFields()
{
    ProcessMetrics metrics;
    metrics.handleCount = 123;
    metrics.gdiObjects = 45;
    metrics.userObjects = 6;
    metrics.workingSetBytes = 64ull * 1024 * 1024;
    metrics.peakWorkingSetBytes = 128ull * 1024 * 1024;
    const QString text = describeMetrics(metrics);
    QVERIFY(text.contains(QStringLiteral("handles=123")));
    QVERIFY(text.contains(QStringLiteral("gdi=45")));
    QVERIFY(text.contains(QStringLiteral("ws=64MB")));
    QVERIFY(text.contains(QStringLiteral("peak=128MB")));
}

void ProcessMetricsTest::exceptionCodesAreTranslated()
{
    QVERIFY(exceptionCodeText(0xC0000005).contains(QStringLiteral("访问冲突")));
    QVERIFY(exceptionCodeText(0xC00000FD).contains(QStringLiteral("栈溢出")));
    QVERIFY(exceptionCodeText(0xC0000374).contains(QStringLiteral("堆损坏")));
    QVERIFY(exceptionCodeText(0xE06D7363).contains(QStringLiteral("C++ 异常")));
    QVERIFY(exceptionCodeText(0x12345678).contains(QStringLiteral("12345678")));
}

QTEST_MAIN(ProcessMetricsTest)

#include "test_process_metrics.moc"
