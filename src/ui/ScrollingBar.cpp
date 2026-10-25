#include "ui/ScrollingBar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "platform/win32/WindowActivation.h"

namespace snapocr::ui {

ScrollingBar::ScrollingBar(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setWindowTitle(QStringLiteral("SnapOCR 滚动长截图"));
    setFixedSize(360, 92);
    setStyleSheet(QStringLiteral(R"(
        QWidget#scrollingBar { background: #FFFFFF; border: 1px solid #E5E8ED; border-radius: 12px; }
        QLabel#scrollingHint { color: #2B2F36; font-size: 13px; }
        QLabel#scrollingProgress { color: #7A818B; font-size: 12px; }
        QPushButton {
            background: #FFFFFF; border: 1px solid #E5E8ED; border-radius: 8px;
            padding: 6px 14px; color: #2B2F36; font-size: 13px;
        }
        QPushButton:hover { border-color: #1F6FEB; color: #1F6FEB; }
        QPushButton#finishButton { background: #1F6FEB; border-color: #1F6FEB; color: #FFFFFF; }
        QPushButton#finishButton:hover { background: #1A5FD0; }
    )"));
    setObjectName(QStringLiteral("scrollingBar"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    m_hint = new QLabel(QStringLiteral("滚动页面，SnapOCR 会自动拼接；完成后点「完成」"), this);
    m_hint->setObjectName(QStringLiteral("scrollingHint"));
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(8);
    m_progress = new QLabel(QStringLiteral("已抓取 0 帧"), this);
    m_progress->setObjectName(QStringLiteral("scrollingProgress"));
    bottom->addWidget(m_progress, 1);

    auto* cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setObjectName(QStringLiteral("cancelScrollingButton"));
    auto* finishButton = new QPushButton(QStringLiteral("完成"), this);
    finishButton->setObjectName(QStringLiteral("finishButton"));
    bottom->addWidget(cancelButton);
    bottom->addWidget(finishButton);
    layout->addLayout(bottom);

    connect(cancelButton, &QPushButton::clicked, this, &ScrollingBar::cancelRequested);
    connect(finishButton, &QPushButton::clicked, this, &ScrollingBar::finishRequested);
}

void ScrollingBar::setProgress(int frames, int stitchedHeight)
{
    if (m_progress != nullptr) {
        m_progress->setText(QStringLiteral("已抓取 %1 帧 · 约 %2 px").arg(frames).arg(stitchedHeight));
    }
}

void ScrollingBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit cancelRequested();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit finishRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace snapocr::ui
