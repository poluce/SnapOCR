#pragma once

#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>
#include <QVector>

#include "ocr/OcrTypes.h"

namespace snapocr::ocr {

// 测试用假 OCR 引擎：不依赖 WinRT，可配置固定结果、失败、延迟与语言列表，
// 用于验证 OcrService 的异步链路、忙时拒绝与超时路径。
//
// 线程安全：recognize() 由 worker 线程调用，配置与观测接口内部均有锁保护。
class FakeOcrEngine final : public IOcrEngine
{
public:
    FakeOcrEngine();
    ~FakeOcrEngine() override = default;

    QString id() const override;
    QVector<OcrLanguage> availableLanguages() const override;
    bool isAvailable(QString* reason = nullptr) const override;
    OcrResult recognize(const QImage& image, const QString& languageTag) override;

    // —— 测试配置（建议在发起识别前设置） ——
    void setResult(const OcrResult& result);
    void setFailure(const QString& errorMessage);
    void setDelayMs(int delayMs);
    void setLanguages(const QVector<OcrLanguage>& languages);
    void setAvailable(bool available, const QString& reason = QString());

    // —— 调用观测 ——
    int recognizeCount() const;
    QString lastLanguageTag() const;
    QSize lastImageSize() const;

private:
    mutable QMutex m_mutex;
    OcrResult m_result;
    QString m_failure;
    bool m_fail = false;
    int m_delayMs = 0;
    bool m_available = true;
    QString m_unavailableReason;
    QVector<OcrLanguage> m_languages;
    int m_recognizeCount = 0;
    QString m_lastLanguageTag;
    QSize m_lastImageSize;
};

}  // namespace snapocr::ocr
