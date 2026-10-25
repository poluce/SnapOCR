#include <QtTest>

#include <QCheckBox>
#include <QFileInfo>
#include <QPlainTextEdit>

#include "ui/OcrResultDialog.h"

using namespace snapocr;
using namespace snapocr::ocr;

// OCR 结果窗（F-OCR-08/09）：默认展示合并后的文本，可切回原始换行；
// 失败时展示可执行提示。
class OcrResultDialogTest final : public QObject
{
    Q_OBJECT

private slots:
    void rendersPreview();
    void showsMergedTextByDefault();
    void canSwitchBackToRawText();
    void failureShowsProblemHint();

private:
    static OcrResult makeResult();
};

OcrResult OcrResultDialogTest::makeResult()
{
    OcrResult result;
    result.ok = true;
    result.fullText = QStringLiteral("第一行\n第二行");
    result.cleanText = QStringLiteral("第一行第二行");
    result.languageTag = QStringLiteral("zh-CN");
    result.elapsedMs = 42;
    OcrLine line1;
    line1.text = QStringLiteral("第一行");
    OcrLine line2;
    line2.text = QStringLiteral("第二行");
    result.lines = {line1, line2};
    return result;
}

void OcrResultDialogTest::showsMergedTextByDefault()
{
    ui::OcrResultDialog dialog;
    dialog.setResult(makeResult());

    auto* edit = dialog.findChild<QPlainTextEdit*>();
    QVERIFY(edit != nullptr);
    QCOMPARE(edit->toPlainText(), QStringLiteral("第一行第二行"));
    QCOMPARE(dialog.text(), QStringLiteral("第一行第二行"));
}

void OcrResultDialogTest::canSwitchBackToRawText()
{
    ui::OcrResultDialog dialog;
    dialog.setResult(makeResult());

    QCheckBox* mergeBox = nullptr;
    for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
        mergeBox = box;  // 只有一个复选框
    }
    QVERIFY(mergeBox != nullptr);
    QVERIFY(mergeBox->isChecked());

    mergeBox->setChecked(false);
    auto* edit = dialog.findChild<QPlainTextEdit*>();
    QCOMPARE(edit->toPlainText(), QStringLiteral("第一行\n第二行"));
}

void OcrResultDialogTest::failureShowsProblemHint()
{
    ui::OcrResultDialog dialog;
    OcrResult failed;
    failed.ok = false;
    failed.errorMessage = QStringLiteral("未安装中文 OCR 语言包");

    const QString hint = QStringLiteral("缺少 OCR 语言包：设置 → 时间和语言 → 语言和区域 → 添加语言");
    dialog.setResult(failed, hint);

    auto* edit = dialog.findChild<QPlainTextEdit*>();
    QVERIFY(edit != nullptr);
    QVERIFY(edit->toPlainText().isEmpty());
    QCOMPARE(dialog.text(), QString());
}

void OcrResultDialogTest::rendersPreview()
{
    ui::OcrResultDialog dialog;
    OcrResult result = makeResult();
    result.fullText = QStringLiteral("SnapOCR 是一个 Windows 截图工具。\n它支持标注与离线 OCR.\n\n"
                                     "第二段：按 F1 截图，按 F4 取字。");
    result.cleanText = QStringLiteral("SnapOCR 是一个 Windows 截图工具。它支持标注与离线 OCR.\n\n"
                                      "第二段：按 F1 截图，按 F4 取字。");
    dialog.setResult(result);
    dialog.resize(560, 360);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QTest::qWait(30);

    const QString path = qEnvironmentVariable("SNAPCR_OCR_PREVIEW");
    if (path.isEmpty()) {
        QSKIP("未设置 SNAPCR_OCR_PREVIEW，跳过文档图渲染");
    }
    dialog.grab().save(path, "PNG");
    QVERIFY(QFileInfo::exists(path));
}

QTEST_MAIN(OcrResultDialogTest)

#include "test_ocr_result_dialog.moc"
