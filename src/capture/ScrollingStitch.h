#pragma once

#include <QImage>
#include <QString>
#include <QVector>

namespace snapocr::capture {

// 滚动长截图拼接（F-CAP-16）。
//
// 约定：用户先框选一个区域，然后向下滚动页面；每抓一帧，新内容相对于旧内容
// 向上移动 offset 像素，即 next[y] == previous[y + offset]。
// 本文件只做纯计算（无窗口/无屏幕依赖），便于单测。

struct StitchResult
{
    bool ok = false;
    QString error;
    QImage image;             // 拼接结果
    QVector<int> offsets;     // 相邻帧的位移（像素）
};

// 在两帧之间寻找垂直重叠位移。
// previous/next 尺寸不同、或找不到足够可信的重叠时返回 false。
// minOverlap: 至少要有多少行重叠才认为是同一次滚动。
// maxOffset: 最大搜索位移（滚动幅度过大时找不到重叠，需要用户放慢速度）。
bool findVerticalOverlap(const QImage& previous, const QImage& next, int minOverlap,
                         int maxOffset, int* offset, double* score);

// 把按时间顺序抓取的帧拼接成一张长图。
// 返回 ok=false 时 error 给出中文原因（例如“两帧之间没有重叠，滚动太快”）。
StitchResult stitchFrames(const QVector<QImage>& frames, int minOverlap = 40);

}  // namespace snapocr::capture
