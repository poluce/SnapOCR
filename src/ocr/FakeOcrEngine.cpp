#include "ocr/FakeOcrEngine.h"

#include <chrono>
#include <thread>

#include <QMutexLocker>

namespace snapocr::ocr {

FakeOcrEngine::FakeOcrEngine()
{
    m_result.ok = true;
    m_languages = {OcrLanguage{QStringLiteral("fake-CN"), QStringLiteral("假引擎·简体中文")}};
    m_unavailableReason = QStringLiteral("测试假引擎被配置为不可用。");
}

QString FakeOcrEngine::id() const
{
    return QStringLiteral("fake");
}

QVector<OcrLanguage> FakeOcrEngine::availableLanguages() const
{
    const QMutexLocker locker(&m_mutex);
    return m_languages;
}

bool FakeOcrEngine::isAvailable(QString* reason) const
{
    const QMutexLocker locker(&m_mutex);
    if (!m_available)
    {
        if (reason != nullptr)
        {
            *reason = m_unavailableReason;
        }
        return false;
    }
    if (reason != nullptr)
    {
        reason->clear();
    }
    return true;
}

OcrResult FakeOcrEngine::recognize(const QImage& image, const QString& languageTag)
{
    int delayMs = 0;
    {
        const QMutexLocker locker(&m_mutex);
        ++m_recognizeCount;
        m_lastLanguageTag = languageTag;
        m_lastImageSize = image.size();
        delayMs = m_delayMs;
    }

    if (delayMs > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    const QMutexLocker locker(&m_mutex);
    OcrResult result = m_result;
    if (m_fail)
    {
        result = OcrResult{};
        result.ok = false;
        result.errorMessage = m_failure;
        return result;
    }
    if (!languageTag.isEmpty())
    {
        result.languageTag = languageTag;
    }
    return result;
}

void FakeOcrEngine::setResult(const OcrResult& result)
{
    const QMutexLocker locker(&m_mutex);
    m_result = result;
    m_fail = false;
}

void FakeOcrEngine::setFailure(const QString& errorMessage)
{
    const QMutexLocker locker(&m_mutex);
    m_fail = true;
    m_failure = errorMessage;
}

void FakeOcrEngine::setDelayMs(int delayMs)
{
    const QMutexLocker locker(&m_mutex);
    m_delayMs = qMax(0, delayMs);
}

void FakeOcrEngine::setLanguages(const QVector<OcrLanguage>& languages)
{
    const QMutexLocker locker(&m_mutex);
    m_languages = languages;
}

void FakeOcrEngine::setAvailable(bool available, const QString& reason)
{
    const QMutexLocker locker(&m_mutex);
    m_available = available;
    m_unavailableReason = reason;
}

int FakeOcrEngine::recognizeCount() const
{
    const QMutexLocker locker(&m_mutex);
    return m_recognizeCount;
}

QString FakeOcrEngine::lastLanguageTag() const
{
    const QMutexLocker locker(&m_mutex);
    return m_lastLanguageTag;
}

QSize FakeOcrEngine::lastImageSize() const
{
    const QMutexLocker locker(&m_mutex);
    return m_lastImageSize;
}

}  // namespace snapocr::ocr
