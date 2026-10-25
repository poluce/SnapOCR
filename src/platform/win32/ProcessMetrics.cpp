#include "platform/win32/ProcessMetrics.h"

#include <windows.h>
#include <psapi.h>

namespace snapocr::platform::win32 {

bool sampleProcessMetrics(ProcessMetrics* metrics)
{
    if (metrics == nullptr) {
        return false;
    }
    const HANDLE process = ::GetCurrentProcess();

    DWORD handles = 0;
    if (::GetProcessHandleCount(process, &handles) != FALSE) {
        metrics->handleCount = handles;
    }
    metrics->gdiObjects = ::GetGuiResources(process, GR_GDIOBJECTS);
    metrics->userObjects = ::GetGuiResources(process, GR_USEROBJECTS);

    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (::GetProcessMemoryInfo(process, &counters, sizeof(counters)) != FALSE) {
        metrics->workingSetBytes = counters.WorkingSetSize;
        metrics->peakWorkingSetBytes = counters.PeakWorkingSetSize;
    }
    return true;
}

QString describeMetrics(const ProcessMetrics& metrics)
{
    return QStringLiteral("handles=%1 gdi=%2 user=%3 ws=%4MB peak=%5MB")
        .arg(metrics.handleCount)
        .arg(metrics.gdiObjects)
        .arg(metrics.userObjects)
        .arg(metrics.workingSetBytes / (1024 * 1024))
        .arg(metrics.peakWorkingSetBytes / (1024 * 1024));
}

}  // namespace snapocr::platform::win32
