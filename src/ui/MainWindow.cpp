#include "ui/MainWindow.h"

#include <QFont>
#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "snapocr/Version.h"

namespace snapocr::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnapOCR"));
    resize(760, 480);
    setMinimumSize(480, 320);

    auto* title = new QLabel(QStringLiteral("SnapOCR"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto* hint = new QLabel(this);
    hint->setText(QStringLiteral(
        "M0 骨架\n\n"
        "截图 / 标注 / OCR 将按路线图逐步落地。\n"
        "下一阶段：M1 屏幕捕获与选区"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);

    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addStretch(1);
    layout->addWidget(title);
    layout->addSpacing(12);
    layout->addWidget(hint);
    layout->addStretch(2);

    auto* central = new QWidget(this);
    central->setLayout(layout);
    setCentralWidget(central);

    statusBar()->showMessage(QStringLiteral("版本 %1 · M0 仓库骨架")
                                 .arg(QString::fromUtf8(snapocr::version::kVersion)));
}

}  // namespace snapocr::ui
