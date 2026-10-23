#include "annotate/AnnotationDocument.h"

#include <QPainter>

#include <algorithm>

namespace snapocr::annotate {

void AnnotationDocument::setBaseImage(const QImage& image)
{
    // 统一成 ARGB32，便于马赛克与合成，且避免渲染时格式转换。
    m_baseImage = image.convertToFormat(QImage::Format_ARGB32);
    m_items.clear();
}

void AnnotationDocument::addItem(const AnnotationItem& item)
{
    m_items.append(item);
}

void AnnotationDocument::removeItem(int index)
{
    if (index >= 0 && index < m_items.size()) {
        m_items.remove(index);
    }
}

void AnnotationDocument::clearItems()
{
    m_items.clear();
}

int AnnotationDocument::hitTest(const QPointF& imagePos) const
{
    // 从后往前找（后画的在上层）。
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const AnnotationItem& item = m_items.at(i);
        const QRectF bounds = computeBounds(item);
        if (bounds.isEmpty()) {
            continue;
        }
        if (bounds.adjusted(-4, -4, 4, 4).contains(imagePos)) {
            return i;
        }
    }
    return -1;
}

void AnnotationDocument::paintAll(QPainter& painter) const
{
    for (const AnnotationItem& item : m_items) {
        paintItem(painter, item, m_baseImage);
    }
}

QImage AnnotationDocument::render() const
{
    if (m_baseImage.isNull()) {
        return {};
    }
    QImage output = m_baseImage.copy();
    QPainter painter(&output);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintAll(painter);
    painter.end();
    return output;
}

int AnnotationDocument::nextStepNumber() const
{
    int maxNumber = 0;
    for (const AnnotationItem& item : m_items) {
        if (item.kind == AnnotationKind::StepNumber) {
            maxNumber = std::max(maxNumber, item.stepNumber);
        }
    }
    return maxNumber + 1;
}

}  // namespace snapocr::annotate
