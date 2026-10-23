#include <QtTest>

#include <QRect>

#include "overlay/SelectionModel.h"

using snapocr::overlay::SelectionModel;

class SelectionModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsNormalizedRect();
    void createClampsToBounds();
    void movesInsideBoundsOnly();
    void resizesFromBottomRight();
    void resizesInvertedHandlesSwaps();
    void nudgesWithinBounds();
    void hitTestsHandlesAndInside();
    void ignoresDragWhenInactive();

private:
    static SelectionModel makeModel();
};

SelectionModel SelectionModelTest::makeModel()
{
    SelectionModel model;
    model.setBounds(QRect(0, 0, 3840, 2160));
    return model;
}

void SelectionModelTest::createsNormalizedRect()
{
    SelectionModel model = makeModel();
    model.beginCreate(QPoint(500, 400));
    model.updateCreate(QPoint(100, 200));
    QCOMPARE(model.rect(), QRect(100, 200, 401, 201));
    QVERIFY(model.hasUsableSelection());
}

void SelectionModelTest::createClampsToBounds()
{
    SelectionModel model = makeModel();
    model.beginCreate(QPoint(50, 50));
    model.updateCreate(QPoint(-500, -500));
    QCOMPARE(model.rect(), QRect(0, 0, 51, 51));
}

void SelectionModelTest::movesInsideBoundsOnly()
{
    SelectionModel model = makeModel();
    model.setRect(QRect(100, 100, 200, 100));
    model.beginMove(QPoint(150, 150));
    model.updateDrag(QPoint(-300, -300));
    QCOMPARE(model.rect(), QRect(0, 0, 200, 100));

    model.beginMove(QPoint(10, 10));
    model.updateDrag(QPoint(5000, 5000));
    const QRect r = model.rect();
    QCOMPARE(r.right(), 3839);
    QCOMPARE(r.bottom(), 2159);
    QCOMPARE(r.size(), QSize(200, 100));
}

void SelectionModelTest::resizesFromBottomRight()
{
    SelectionModel model = makeModel();
    model.setRect(QRect(100, 100, 200, 100));  // right=299 bottom=199
    model.beginResize(SelectionModel::Handle::BottomRight, QPoint(299, 199));
    model.updateDrag(QPoint(399, 299));
    QCOMPARE(model.rect(), QRect(100, 100, 300, 200));
}

void SelectionModelTest::resizesInvertedHandlesSwaps()
{
    SelectionModel model = makeModel();
    model.setRect(QRect(100, 100, 200, 100));
    model.beginResize(SelectionModel::Handle::Left, QPoint(100, 150));
    model.updateDrag(QPoint(400, 150));
    const QRect r = model.rect();
    QVERIFY(r.isValid());
    QVERIFY(r.width() >= 1);
    QCOMPARE(r.left(), 299);
    QCOMPARE(r.right(), 400);
}

void SelectionModelTest::nudgesWithinBounds()
{
    SelectionModel model = makeModel();
    model.setRect(QRect(0, 0, 100, 100));
    model.nudge(-10, -10);
    QCOMPARE(model.rect(), QRect(0, 0, 100, 100));

    model.setRect(QRect(1000, 1000, 100, 100));
    model.nudge(10, 5);
    QCOMPARE(model.rect(), QRect(1010, 1005, 100, 100));
}

void SelectionModelTest::hitTestsHandlesAndInside()
{
    SelectionModel model = makeModel();
    model.setRect(QRect(100, 100, 200, 100));

    QCOMPARE(model.hitTest(QPoint(100, 100), 8), SelectionModel::Handle::TopLeft);
    QCOMPARE(model.hitTest(QPoint(300, 200), 8), SelectionModel::Handle::BottomRight);
    QCOMPARE(model.hitTest(QPoint(200, 150), 8), SelectionModel::Handle::Inside);
    QCOMPARE(model.hitTest(QPoint(1000, 1000), 8), SelectionModel::Handle::None);
}

void SelectionModelTest::ignoresDragWhenInactive()
{
    SelectionModel model = makeModel();
    model.updateCreate(QPoint(10, 10));
    model.updateDrag(QPoint(20, 20));
    QVERIFY(!model.isActive());
    QVERIFY(model.rect().isEmpty());
}

QTEST_GUILESS_MAIN(SelectionModelTest)

#include "test_selection_model.moc"
