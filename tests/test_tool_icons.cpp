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
        QVERIFY(pixmap.devicePixelRatio() >= 2.0);
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

QTEST_MAIN(ToolIconsTest)

#include "test_tool_icons.moc"
