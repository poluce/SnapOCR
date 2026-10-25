#include "ui/Theme.h"
#include "ui/OcrResultDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace snapocr::ui {

OcrResultDialog::OcrResultDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("提取文字"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(560, 420);

    applyStyleSheet();
    connect(&ui::ThemeNotifier::instance(), &ui::ThemeNotifier::themeChanged, this,
            &OcrResultDialog::applyStyleSheet);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(QStringLiteral("识别结果会显示在这里"));
    layout->addWidget(m_textEdit, 1);

    m_mergeParagraphs = new QCheckBox(QStringLiteral("合并段落（清理排版）"), this);
    m_mergeParagraphs->setChecked(true);
    m_mergeParagraphs->setToolTip(
        QStringLiteral("把被排版拆断的行合并成段落；取消勾选可保留引擎原始换行"));
    layout->addWidget(m_mergeParagraphs);

    auto* buttons = new QDialogButtonBox(this);
    m_retryButton = buttons->addButton(QStringLiteral("重新识别"), QDialogButtonBox::ActionRole);
    m_copyButton = buttons->addButton(QStringLiteral("复制"), QDialogButtonBox::ActionRole);
    QPushButton* closeButton = buttons->addButton(QStringLiteral("关闭"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        const QString content = m_textEdit->toPlainText();
        if (content.isEmpty()) {
            return;
        }
        QApplication::clipboard()->setText(content);
        emit copyRequested(content);
        m_infoLabel->setText(QStringLiteral("已复制到剪贴板"));
    });
    connect(m_mergeParagraphs, &QCheckBox::toggled, this, [this](bool) {
        if (m_result.ok) {
            setResult(m_result, m_problemHint);
        }
    });
    connect(m_retryButton, &QPushButton::clicked, this, &OcrResultDialog::retryRequested);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void OcrResultDialog::setResult(const ocr::OcrResult& result, const QString& problemHint)
{
    m_result = result;
    m_problemHint = problemHint;

    if (!result.ok) {
        m_infoLabel->setText(m_problemHint.isEmpty()
                                 ? QStringLiteral("识别失败：%1").arg(result.errorMessage)
                                 : m_problemHint);
        m_textEdit->setPlainText(QString());
        m_mergeParagraphs->setEnabled(false);
        return;
    }

    m_mergeParagraphs->setEnabled(!result.cleanText.isEmpty()
                                 && result.cleanText != result.fullText);

    const QString& text = m_mergeParagraphs->isChecked() && !result.cleanText.isEmpty()
                              ? result.cleanText
                              : result.fullText;
    m_textEdit->setPlainText(text);

    if (result.fullText.trimmed().isEmpty()) {
        m_infoLabel->setText(m_problemHint.isEmpty()
                                 ? QStringLiteral("未识别到文字，可尝试重新框选或改用更大的区域")
                                 : m_problemHint);
        return;
    }

    QString info = QStringLiteral("语言：%1 · 耗时：%2ms · %3 行")
                       .arg(result.languageTag.isEmpty() ? QStringLiteral("系统默认")
                                                         : result.languageTag)
                       .arg(result.elapsedMs)
                       .arg(result.lines.size());
    if (m_mergeParagraphs->isChecked() && !result.cleanText.isEmpty()
        && result.cleanText != result.fullText) {
        info += QStringLiteral(" · 已合并段落");
    }
    m_infoLabel->setText(info);
}

void OcrResultDialog::setBusy(const QString& message)
{
    m_infoLabel->setText(message);
    m_textEdit->setPlainText(QString());
    m_copyButton->setEnabled(false);
}

QString OcrResultDialog::text() const
{
    return m_textEdit->toPlainText();
}


void OcrResultDialog::applyStyleSheet()
{
    setStyleSheet(ui::styled(QStringLiteral(R"(
        QDialog { background: {window}; }
        QLabel { color: #5A616B; font-size: 12px; }
        QPlainTextEdit {
            background: {surface}; border: 1px solid {border}; border-radius: 10px;
            padding: 8px; color: #2B2F36; font-size: 13px; selection-background-color: {accent};
        }
        QCheckBox { color: #2B2F36; font-size: 12px; }
        QPushButton {
            background: {surface}; border: 1px solid {border}; border-radius: 8px;
            padding: 6px 14px; color: #2B2F36; font-size: 13px;
        }
        QPushButton:hover { border-color: {accent}; color: {accent}; }
        QPushButton:pressed { background: {accentSoft}; }
    )")));
}

}  // namespace snapocr::ui
