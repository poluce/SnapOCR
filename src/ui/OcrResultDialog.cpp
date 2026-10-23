#include "ui/OcrResultDialog.h"

#include <QApplication>
#include <QClipboard>
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

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(QStringLiteral("识别结果会显示在这里"));
    layout->addWidget(m_textEdit, 1);

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
    connect(m_retryButton, &QPushButton::clicked, this, &OcrResultDialog::retryRequested);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void OcrResultDialog::setResult(const ocr::OcrResult& result)
{
    if (!result.ok) {
        m_infoLabel->setText(QStringLiteral("识别失败：%1").arg(result.errorMessage));
        m_textEdit->setPlainText(QString());
        return;
    }

    m_textEdit->setPlainText(result.fullText);
    QString info = QStringLiteral("语言：%1 · 耗时：%2ms · %3 行")
                       .arg(result.languageTag.isEmpty() ? QStringLiteral("系统默认")
                                                         : result.languageTag)
                       .arg(result.elapsedMs)
                       .arg(result.lines.size());
    if (result.fullText.trimmed().isEmpty()) {
        info = QStringLiteral("未识别到文字，可尝试重新框选或改用更大的区域");
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

}  // namespace snapocr::ui
