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

    // F-ANN-17：缩放/层级。targetBounds 是图元新的包围盒（图像像素）。
    bool scaleItem(int index, const QRectF& targetBounds);
    bool moveItemToTop(int index);
    bool moveItemToBottom(int index);
    bool moveItemUp(int index);
    bool moveItemDown(int index);

    // 合成输出：底图 + 全部图元，尺寸与底图一致（物理像素 1:1）。
    QImage render() const;

    // 把全部图元绘制到指定 painter（画布复用，painter 坐标系为图像像素）。
    void paintAll(QPainter& painter) const;

    // 序号标注的下一个序号（已有最大序号 + 1）。
    int nextStepNumber() const;

private:
    QImage m_baseImage;
    QVector<AnnotationItem> m_items;
    // 模糊笔刷需要底图的模糊版本，按需生成并缓存（底图变化时失效）
    mutable QImage m_blurredBase;
    mutable bool m_blurredBaseReady = false;
};

}  // namespace snapocr::annotate
