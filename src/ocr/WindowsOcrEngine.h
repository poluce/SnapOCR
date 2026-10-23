#pragma once

#include <QString>
#include <QVector>

#include "ocr/OcrTypes.h"

namespace snapocr::ocr {

// Windows 系统 OCR 引擎（Windows.Media.Ocr / C++/WinRT）。
//
// 约定：
// - recognize() 必须在 worker 线程调用，内部会初始化多线程 COM 公寓并阻塞等待异步识别；
// - 本类不缓存每次识别的状态，可在多个 worker 线程并发调用；
// - 所有 WinRT 异常都在内部转换为中文错误信息，绝不向外抛出，也不会触发崩溃；
// - 语言枚举/可用性检查可以（也通常）在主线程调用，内部会以单线程公寓初始化。
class WindowsOcrEngine final : public IOcrEngine
{
public:
    WindowsOcrEngine();
    ~WindowsOcrEngine() override;

    QString id() const override;
    QVector<OcrLanguage> availableLanguages() const override;
    bool isAvailable(QString* reason = nullptr) const override;
    OcrResult recognize(const QImage& image, const QString& languageTag) override;
};

}  // namespace snapocr::ocr
