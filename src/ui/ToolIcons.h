#pragma once

#include <QColor>
#include <QIcon>

#include "annotate/AnnotationTypes.h"

namespace snapocr::ui {

// 程序化绘制图标，避免引入图标资源与第三方素材的许可证问题。
// color 默认深色（浅色主题 UI 用）；截图工具条为深色底，需传浅色。
QIcon toolIcon(annotate::AnnotationKind kind, const QColor& color = QColor(0x33, 0x33, 0x33));
QIcon actionIcon(const QString& action, const QColor& color = QColor(0x33, 0x33, 0x33));

}  // namespace snapocr::ui
