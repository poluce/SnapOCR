#include "annotate/AnnotationDocument.h"

#include "annotate/ImageBlur.h"

#include <QPainter>

#include <algorithm>

namespace snapocr::annotate {

void AnnotationDocument::setBaseImage(const QImage& image)
{
    m_blurredBaseReady = false;
    m_blurredBase = QImage();
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

namespace {

// 图元内容包围盒：只由点坐标决定（不含线宽/字体 padding），用于缩放基准。
QRectF contentBounds(const AnnotationItem& item)
{
    switch (item.kind) {
    case AnnotationKind::Rectangle:
    case AnnotationKind::Ellipse:
    case AnnotationKind::Highlight:
    case AnnotationKind::Mosaic:
        if (item.points.size() < 2) {
            return {};
        }
        return QRectF(item.points[0], item.points[1]).normalized();
    case AnnotationKind::Line:
    case AnnotationKind::Arrow:
    case AnnotationKind::Freehand:
    case AnnotationKind::Blur: {
        if (item.points.isEmpty()) {
            return {};
        }
        qreal minX = item.points.first().x();
        qreal maxX = minX;
        qreal minY = item.points.first().y();
        qreal maxY = minY;
        for (const QPointF& point : item.points) {
            minX = std::min(minX, point.x());
            maxX = std::max(maxX, point.x());
            minY = std::min(minY, point.y());
            maxY = std::max(maxY, point.y());
        }
        return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }
    case AnnotationKind::Text:
    case AnnotationKind::StepNumber:
        return computeBounds(item);
    }
    return {};
}

}  // namespace

bool AnnotationDocument::scaleItem(int index, const QRectF& targetBounds)
{
    if (index < 0 || index >= m_items.size() || targetBounds.isEmpty()) {
        return false;
    }
    AnnotationItem& item = m_items[index];
    const QRectF current = contentBounds(item);
    if (!current.isValid() || current.width() <= 0 || current.height() <= 0) {
        return false;
    }
    const qreal scaleX = targetBounds.width() / current.width();
    const qreal scaleY = targetBounds.height() / current.height();

    auto mapPoint = [&](const QPointF& point) {
        return QPointF(targetBounds.left() + (point.x() - current.left()) * scaleX,
                       targetBounds.top() + (point.y() - current.top()) * scaleY);
    };

    switch (item.kind) {
    case AnnotationKind::Rectangle:
    case AnnotationKind::Ellipse:
    case AnnotationKind::Highlight:
    case AnnotationKind::Mosaic: {
        // 两角点直接对应新包围盒
        const bool reversedX = item.points.size() >= 2 && item.points[1].x() < item.points[0].x();
        const bool reversedY = item.points.size() >= 2 && item.points[1].y() < item.points[0].y();
        const QPointF topLeft = targetBounds.topLeft();
        const QPointF bottomRight = targetBounds.bottomRight();
        item.points = {reversedX ? bottomRight : topLeft, reversedX ? topLeft : bottomRight};
        if (!reversedY) {
            item.points[0].setY(topLeft.y());
            item.points[1].setY(bottomRight.y());
        }
        break;
    }
    case AnnotationKind::Line:
    case AnnotationKind::Arrow:
    case AnnotationKind::Freehand:
    case AnnotationKind::Blur: {
        for (QPointF& point : item.points) {
            point = mapPoint(point);
        }
        break;
    }
    case AnnotationKind::Text: {
        item.points = {mapPoint(item.points.value(0))};
        item.style.fontSize = qMax(6, qRound(item.style.fontSize * qMin(scaleX, scaleY)));
        break;
    }
    case AnnotationKind::StepNumber: {
        item.points = {mapPoint(item.points.value(0))};
        item.style.fontSize = qMax(6, qRound(item.style.fontSize * qMin(scaleX, scaleY)));
        break;
    }
    }
    return true;
}

bool AnnotationDocument::moveItemToTop(int index)
{
    if (index < 0 || index >= m_items.size() || index == m_items.size() - 1) {
        return false;
    }
    const AnnotationItem item = m_items.takeAt(index);
    m_items.append(item);
    return true;
}

bool AnnotationDocument::moveItemToBottom(int index)
{
    if (index <= 0 || index >= m_items.size()) {
        return false;
    }
    const AnnotationItem item = m_items.takeAt(index);
    m_items.prepend(item);
    return true;
}

bool AnnotationDocument::moveItemUp(int index)
{
    if (index < 0 || index >= m_items.size() - 1) {
        return false;
    }
    m_items.swapItemsAt(index, index + 1);
    return true;
}

bool AnnotationDocument::moveItemDown(int index)
{
    if (index <= 0 || index >= m_items.size()) {
        return false;
    }
    m_items.swapItemsAt(index, index - 1);
    return true;
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
    QImage blurred;
    for (const AnnotationItem& item : m_items) {
        if (item.kind == AnnotationKind::Blur && item.style.blurRadius > 0) {
            if (!m_blurredBaseReady) {
                m_blurredBase = gaussianBlur(m_baseImage, item.style.blurRadius);
                m_blurredBaseReady = true;
            }
            blurred = m_blurredBase;
            break;
        }
    }
    for (const AnnotationItem& item : m_items) {
        paintItem(painter, item, m_baseImage, blurred);
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
