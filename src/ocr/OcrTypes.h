#pragma once

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>

namespace snapocr::ocr {

struct OcrLanguage
{
    QString tag;          // 如 zh-Hans-CN
    QString displayName;  // 如 简体中文(中国大陆)
};

// 一行识别结果。boundingBox 使用输入图像的像素坐标（左上角原点）。
struct OcrLine
{
    QString text;
    QRectF boundingBox;
};

struct OcrResult
{
    bool ok = false;
    QString errorMessage;
    QString languageTag;
    QString fullText;
    QVector<OcrLine> lines;
    qint64 elapsedMs = 0;
};

// OCR 引擎抽象。业务层只依赖本接口，不得直接使用 winrt:: 类型。
// recognize() 在 worker 线程执行，实现内部允许阻塞；禁止在实现中触碰 UI。
class IOcrEngine
{
public:
    virtual ~IOcrEngine() = default;

    virtual QString id() const = 0;

    // 当前系统可用的识别语言；空表示不可用。
    virtual QVector<OcrLanguage> availableLanguages() const = 0;

    // 引擎是否可用；reason 返回中文原因（可为 nullptr）。
    virtual bool isAvailable(QString* reason = nullptr) const = 0;

    // 识别整张图像；languageTag 为空时使用默认语言。
    virtual OcrResult recognize(const QImage& image, const QString& languageTag) = 0;
};

}  // namespace snapocr::ocr
