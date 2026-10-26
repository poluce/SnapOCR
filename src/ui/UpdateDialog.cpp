#include "ui/UpdateDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace snapocr::ui {

UpdateDialog::UpdateDialog(const update::UpdateRelease& release, const QString& currentVersion,
                           QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
    setWindowTitle(tr("发现新版本"));
    setModal(true);
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("SnapOCR %1 可用").arg(release.version), this);
    title->setObjectName(QStringLiteral("updateTitle"));
    layout->addWidget(title);

    auto* versions =
        new QLabel(tr("当前版本 %1 → 新版本 %2").arg(currentVersion, release.version), this);
    versions->setObjectName(QStringLiteral("updateVersions"));
    layout->addWidget(versions);

    qint64 size = release.setupSize > 0 ? release.setupSize : release.zipSize;
    m_sizeLabel = new QLabel(this);
    m_sizeLabel->setObjectName(QStringLiteral("updateSize"));
    if (size > 0) {
        m_sizeLabel->setText(tr("安装包大小：%1 MB").arg(QString::number(size / 1024.0 / 1024.0, 'f', 1)));
    } else {
        m_sizeLabel->setText(tr("安装包大小：未知"));
    }
    layout->addWidget(m_sizeLabel);

    auto* notesTitle = new QLabel(tr("更新说明"), this);
    notesTitle->setObjectName(QStringLiteral("updateSection"));
    layout->addWidget(notesTitle);

    m_notes = new QPlainTextEdit(this);
    m_notes->setReadOnly(true);
    m_notes->setPlainText(release.notes.trimmed().isEmpty()
                              ? tr("（该版本没有附带更新说明）")
                              : release.notes.trimmed());
    m_notes->setFixedHeight(140);
    layout->addWidget(m_notes);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    auto* skipButton = new QPushButton(tr("跳过此版本"), this);
    auto* laterButton = new QPushButton(tr("稍后提醒"), this);
    auto* downloadButton = new QPushButton(tr("下载并安装"), this);
    downloadButton->setDefault(true);
    buttons->addWidget(skipButton);
    buttons->addStretch(1);
    buttons->addWidget(laterButton);
    buttons->addWidget(downloadButton);
    layout->addLayout(buttons);

    connect(skipButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::SkipVersion;
        accept();
    });
    connect(laterButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::RemindLater;
        reject();
    });
    connect(downloadButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::DownloadAndInstall;
        accept();
    });

    connect(&ui::ThemeNotifier::instance(), &ui::ThemeNotifier::themeChanged, this,
            &UpdateDialog::applyStyleSheet);
    applyStyleSheet();
}

void UpdateDialog::applyStyleSheet()
{
    const QString qss = ui::styled(QStringLiteral(
        "QDialog { background: {window}; }\n"
        "QLabel#updateTitle { color: {text}; font-size: 17px; font-weight: 600; }\n"
        "QLabel#updateVersions { color: {textMuted}; font-size: 13px; }\n"
        "QLabel#updateSize { color: {textFaint}; font-size: 12px; }\n"
        "QLabel#updateSection { color: {text}; font-size: 13px; font-weight: 600; }\n"
        "QPlainTextEdit { background: {surface}; border: 1px solid {border};"
        " border-radius: 8px; padding: 8px; color: {text}; font-size: 13px; }\n"
        "QPushButton { background: {surface}; border: 1px solid {border};"
        " border-radius: 8px; padding: 7px 18px; color: {text}; font-size: 13px; }\n"
        "QPushButton:hover { background: {surfaceHover}; }\n"
        "QPushButton:default { background: {accent}; border-color: {accent}; color: white; }\n"
        "QPushButton:default:hover { background: {accent}; }\n"));
    setStyleSheet(qss);
}

}  // namespace snapocr::ui
