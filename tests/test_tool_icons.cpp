#include <QtTest>

#include <QIcon>
#include <QImage>
#include <QPixmap>

#include "annotate/AnnotationTypes.h"
#include "ui/ToolIcons.h"

using namespace snapocr;

// 回归：图标必须按高分辨率绘制并带 devicePixelRatio，
// 否则在高 DPI（150%/200%）下会被放大，看起来发虚。
class ToolIconsTest final : public QObject
{
    Q_OBJECT

private slots:
    void toolIconsAreHighResolution();
    void actionIconsAreHighResolution();
    void iconSizeIsTwentyLogicalPixels();
    void iconsProvideMultiplePhysicalSizes();
};

void ToolIconsTest::toolIconsAreHighResolution()
{
    const QList<annotate::AnnotationKind> kinds = {
        annotate::AnnotationKind::Rectangle, annotate::AnnotationKind::Ellipse,
        annotate::AnnotationKind::Line,      annotate::AnnotationKind::Arrow,
        annotate::AnnotationKind::Freehand,  annotate::AnnotationKind::Text,
        annotate::AnnotationKind::StepNumber, annotate::AnnotationKind::Highlight,
        annotate::AnnotationKind::Mosaic};
    for (annotate::AnnotationKind kind : kinds) {
        const QIcon icon = ui::toolIcon(kind, QColor(0x2B, 0x2F, 0x36));
        // 在 200% 缩放下取图：应当是 40x40 物理像素的高清图，而不是 20x20 被放大。
        const QPixmap pixmap = icon.pixmap(QSize(20, 20), 2.0);
        QVERIFY(!pixmap.isNull());
        QVERIFY2(pixmap.width() >= 40,
                 qPrintable(QStringLiteral("high-dpi icon width=%1").arg(pixmap.width())));
        QCOMPARE(pixmap.width() / pixmap.devicePixelRatio(), 20.0);
    }
}

void ToolIconsTest::actionIconsAreHighResolution()
{
    const QStringList actions = {QStringLiteral("select"),  QStringLiteral("undo"),
                                 QStringLiteral("redo"),    QStringLiteral("copy"),
                                 QStringLiteral("save"),    QStringLiteral("ocr"),
                                 QStringLiteral("close"),   QStringLiteral("confirm")};
    for (const QString& action : actions) {
        const QIcon icon = ui::actionIcon(action, QColor(0x2B, 0x2F, 0x36));
        const QPixmap pixmap = icon.pixmap(QSize(20, 20), 2.0);
        QVERIFY2(!pixmap.isNull(), qPrintable(action));
        QVERIFY2(pixmap.width() >= 40, qPrintable(action));
    }
}

void ToolIconsTest::iconSizeIsTwentyLogicalPixels()
{
    const QIcon icon = ui::toolIcon(annotate::AnnotationKind::Rectangle, Qt::black);
    const QPixmap pixmap = icon.pixmap(QSize(20, 20), 2.0);
    // 逻辑尺寸仍是 20（物理尺寸 = 20 * dpr）
    const qreal logical = pixmap.width() / pixmap.devicePixelRatio();
    QVERIFY2(qAbs(logical - 20.0) < 1.5,
             qPrintable(QStringLiteral("logical=%1").arg(logical)));
}

// 关键回归：QIcon 必须同时持有 1x/2x/3x/4x 的实际像素尺寸。
// 只放一张 20x20（DPR=1）会放大发虚；只放一张带 DPR 标记的图会让 Qt 取错尺寸
// （200% 缩放下只剩左上角碎片）。这里是这两次事故的护栏。
void ToolIconsTest::iconsProvideMultiplePhysicalSizes()
{
    const QIcon icon = ui::toolIcon(annotate::AnnotationKind::Rectangle, QColor(0x2B, 0x2F, 0x36));
    const QList<QSize> sizes = icon.availableSizes();
    QVERIFY(sizes.size() >= 4);
    for (int scale : {1, 2, 3, 4}) {
        const QSize expected(20 * scale, 20 * scale);
        QVERIFY2(sizes.contains(expected),
                 qPrintable(QStringLiteral("missing icon size %1x%2")
                                .arg(expected.width())
                                .arg(expected.height())));
    }
    // 每个条目都是 DPR=1 的实体像素（不靠 DPR 标记骗 Qt）
    const QPixmap at1x = icon.pixmap(QSize(20, 20), 1.0);
    QCOMPARE(at1x.width(), 20);
    const QPixmap at2x = icon.pixmap(QSize(20, 20), 2.0);
    QCOMPARE(at2x.width(), 40);
}

QTEST_MAIN(ToolIconsTest)

#include "test_tool_icons.moc"
