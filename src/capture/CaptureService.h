#pragma once

#include <QObject>
#include <QString>

#include "capture/CaptureTypes.h"

namespace snapocr::capture {

// 屏幕捕获服务：同步抓取全部显示器（GDI 拷贝通常 20–40ms），完成后经信号返回。
// 调用方必须在创建任何覆盖层窗口之前调用，保证不会把自身窗口截进图里。
class CaptureService : public QObject
{
    Q_OBJECT

public:
    explicit CaptureService(QObject* parent = nullptr);

    // 抓取所有显示器；失败时发出 failed()。
    // includeCursor：把鼠标光标画进截图（设置项 capture/includeCursor）。
    void captureAllScreens(bool includeCursor = false);

signals:
    void captured(const snapocr::capture::CaptureResult& result);
    void failed(const QString& message);
};

}  // namespace snapocr::capture
