#pragma once

#include <QImage>
#include <QPointF>
#include <QSize>
#include <QVector>

#include "annotate/AnnotationTypes.h"

namespace snapocr::annotate {

// 标注文档：底图（选区裁剪后的物理像素图）+ 有序图元列表。
// 撤销/重做由编辑器用 QUndoStack + 快照实现，本类只负责数据与渲染。
class AnnotationDocument
{
public:
    AnnotationDocument() = default;

    void setBaseImage(const QImage& image);
    const QImage& baseImage() const { return m_baseImage; }
    QSize imageSize() const { return m_baseImage.size(); }

    const QVector<AnnotationItem>& items() const { return m_items; }
    void setItems(const QVector<AnnotationItem>& items) { m_items = items; }
    void addItem(const AnnotationItem& item);
    void removeItem(int index);
    void clearItems();

    int hitTest(const QPointF& imagePos) const;

    // 合成输出：底图 + 全部图元，尺寸与底图一致（物理像素 1:1）。
    QImage render() const;

    // 把全部图元绘制到指定 painter（画布复用，painter 坐标系为图像像素）。
    void paintAll(QPainter& painter) const;

    // 序号标注的下一个序号（已有最大序号 + 1）。
    int nextStepNumber() const;

private:
    QImage m_baseImage;
    QVector<AnnotationItem> m_items;
};

}  // namespace snapocr::annotate
