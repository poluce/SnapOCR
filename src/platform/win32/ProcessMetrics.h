#pragma once

#include <QString>

namespace snapocr::platform::win32 {

// 进程资源快照（阶段三 §8.2 稳定性审计用）。
struct ProcessMetrics
{
    quint32 handleCount = 0;       // 内核句柄数
    quint32 gdiObjects = 0;        // GDI 对象数（DC/位图/画笔）
    quint32 userObjects = 0;       // USER 对象数（窗口/菜单）
    quint64 workingSetBytes = 0;   // 当前工作集
    quint64 peakWorkingSetBytes = 0;  // 峰值工作集
};

bool sampleProcessMetrics(ProcessMetrics* metrics);

// 供 CLI 自检输出的一行描述。
QString describeMetrics(const ProcessMetrics& metrics);

}  // namespace snapocr::platform::win32
