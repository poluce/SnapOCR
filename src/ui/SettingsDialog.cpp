#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "app/HotkeyManager.h"

namespace snapocr::ui {

SettingsDialog::SettingsDialog(const common::AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("SnapOCR 设置"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(520, 460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(8);

    m_captureHotkey = new QKeySequenceEdit(this);
    m_captureHotkey->setMaximumSequenceLength(1);
    m_ocrHotkey = new QKeySequenceEdit(this);
    m_ocrHotkey->setMaximumSequenceLength(1);
    form->addRow(QStringLiteral("截图热键："), m_captureHotkey);
    form->addRow(QStringLiteral("取字热键："), m_ocrHotkey);

    auto* dirRow = new QWidget(this);
    auto* dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(6);
    m_saveDirectory = new QLineEdit(dirRow);
    m_saveDirectory->setPlaceholderText(QStringLiteral("留空则使用 图片\\SnapOCR"));
    auto* browseButton = new QPushButton(QStringLiteral("浏览…"), dirRow);
    dirLayout->addWidget(m_saveDirectory, 1);
    dirLayout->addWidget(browseButton);
    form->addRow(QStringLiteral("保存目录："), dirRow);
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择保存目录"), m_saveDirectory->text());
        if (!dir.isEmpty()) {
            m_saveDirectory->setText(dir);
        }
    });

    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG（无损）"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPG（体积小）"), QStringLiteral("jpg"));
    form->addRow(QStringLiteral("保存格式："), m_format);

    m_jpgQuality = new QSpinBox(this);
    m_jpgQuality->setRange(50, 100);
    m_jpgQuality->setSuffix(QStringLiteral(" %"));
    form->addRow(QStringLiteral("JPG 质量："), m_jpgQuality);

    m_nameTemplate = new QLineEdit(this);
    m_nameTemplate->setPlaceholderText(QStringLiteral("SnapOCR_yyyyMMdd_HHmmss"));
    form->addRow(QStringLiteral("文件名模板："), m_nameTemplate);

    m_theme = new QComboBox(this);
    m_theme->addItem(QStringLiteral("跟随系统"), QStringLiteral("system"));
    m_theme->addItem(QStringLiteral("浅色"), QStringLiteral("light"));
    m_theme->addItem(QStringLiteral("深色"), QStringLiteral("dark"));
    form->addRow(QStringLiteral("主题："), m_theme);

    m_autoStart = new QCheckBox(QStringLiteral("开机自动启动"), this);
    form->addRow(QString(), m_autoStart);

    m_ocrAutoCopy = new QCheckBox(QStringLiteral("识别后自动复制到剪贴板"), this);
    form->addRow(QString(), m_ocrAutoCopy);

    m_logLevel = new QComboBox(this);
    m_logLevel->addItem(QStringLiteral("错误"), QStringLiteral("error"));
    m_logLevel->addItem(QStringLiteral("警告"), QStringLiteral("warning"));
    m_logLevel->addItem(QStringLiteral("信息"), QStringLiteral("info"));
    m_logLevel->addItem(QStringLiteral("调试"), QStringLiteral("debug"));
    form->addRow(QStringLiteral("日志级别："), m_logLevel);

    layout->addLayout(form);
    layout->addStretch(1);

    auto* hint = new QLabel(
        QStringLiteral("提示：热键被其他程序占用时会自动回退并提示，可在此更换。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        // 热键合法性校验：无法映射到 Win32 的组合直接拒绝。
        const QKeySequence capture = m_captureHotkey->keySequence();
        const QKeySequence ocr = m_ocrHotkey->keySequence();
        if (!capture.isEmpty()) {
            QString error;
            if (!app::HotkeyManager::toWin32(capture, nullptr, nullptr, &error)
                && !error.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("热键不可用"), error);
                return;
            }
        }
        if (!ocr.isEmpty()) {
            QString error;
            if (!app::HotkeyManager::toWin32(ocr, nullptr, nullptr, &error) && !error.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("热键不可用"), error);
                return;
            }
        }
        collectFromUi();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadIntoUi(settings);
}

void SettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // 首次显示时把窗口夹取到所在屏幕的可用区域内：
    // QDialog 默认按父窗口居中，父窗口贴边时对话框会被推出屏幕。
    const QRect frameBefore = frameGeometry();
    QScreen* screen = QGuiApplication::screenAt(frameBefore.center());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    const QRect available = screen->availableGeometry();
    QRect target = frameBefore;
    if (target.right() > available.right()) {
        target.moveRight(available.right() - 8);
    }
    if (target.bottom() > available.bottom()) {
        target.moveBottom(available.bottom() - 8);
    }
    if (target.left() < available.left()) {
        target.moveLeft(available.left() + 8);
    }
    if (target.top() < available.top()) {
        target.moveTop(available.top() + 8);
    }
    if (target.topLeft() != frameBefore.topLeft()) {
        move(target.topLeft());
    }
}

void SettingsDialog::loadIntoUi(const common::AppSettings& settings)
{
    m_captureHotkey->setKeySequence(QKeySequence(settings.captureHotkey));
    m_ocrHotkey->setKeySequence(QKeySequence(settings.ocrHotkey));
    m_saveDirectory->setText(settings.saveDirectory);
    const int formatIndex = m_format->findData(settings.saveFormat);
    m_format->setCurrentIndex(formatIndex < 0 ? 0 : formatIndex);
    m_jpgQuality->setValue(settings.jpgQuality);
    m_nameTemplate->setText(settings.nameTemplate);
    const int themeIndex = m_theme->findData(settings.theme);
    m_theme->setCurrentIndex(themeIndex < 0 ? 0 : themeIndex);
    m_autoStart->setChecked(settings.startWithWindows);
    m_ocrAutoCopy->setChecked(settings.ocrAutoCopy);
    const int levelIndex = m_logLevel->findData(settings.logLevel);
    m_logLevel->setCurrentIndex(levelIndex < 0 ? 2 : levelIndex);
}

void SettingsDialog::collectFromUi()
{
    const QKeySequence capture = m_captureHotkey->keySequence();
    const QKeySequence ocr = m_ocrHotkey->keySequence();
    m_settings.captureHotkey =
        capture.isEmpty() ? QStringLiteral("F1") : capture.toString(QKeySequence::PortableText);
    m_settings.ocrHotkey =
        ocr.isEmpty() ? QStringLiteral("F4") : ocr.toString(QKeySequence::PortableText);
    m_settings.saveDirectory = m_saveDirectory->text().trimmed();
    m_settings.saveFormat = m_format->currentData().toString();
    m_settings.jpgQuality = m_jpgQuality->value();
    m_settings.nameTemplate = m_nameTemplate->text().trimmed();
    m_settings.theme = m_theme->currentData().toString();
    m_settings.startWithWindows = m_autoStart->isChecked();
    m_settings.ocrAutoCopy = m_ocrAutoCopy->isChecked();
    m_settings.logLevel = m_logLevel->currentData().toString();
}

common::AppSettings SettingsDialog::settings() const
{
    return m_settings;
}

}  // namespace snapocr::ui
