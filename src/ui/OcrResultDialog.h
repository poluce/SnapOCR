#pragma once

#include <QDialog>
#include <QString>

#include "ocr/OcrTypes.h"

class QPlainTextEdit;
class QLabel;
class QPushButton;

namespace snapocr::ui {

// OCR 结果窗：展示识别文本（可编辑修正）、语言与耗时、复制/关闭。
class OcrResultDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit OcrResultDialog(QWidget* parent = nullptr);

    void setResult(const snapocr::ocr::OcrResult& result);
    void setBusy(const QString& message);
    QString text() const;

signals:
    void copyRequested(const QString& text);
    void retryRequested();

private:
    QPlainTextEdit* m_textEdit = nullptr;
    QLabel* m_infoLabel = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_retryButton = nullptr;
};

}  // namespace snapocr::ui
