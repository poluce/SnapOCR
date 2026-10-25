#include "ui/SettingsDialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QShowEvent>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include "app/HotkeyManager.h"
#include "ui/TitleBar.h"
#include "ui/ToggleSwitch.h"

namespace snapocr::ui {
namespace {

constexpr int kDialogWidth = 560;
constexpr int kShadowMargin = 16;

// 主题分段控件的选项（key, 显示名）
struct ThemeOption
{
    const char* key;
    const char* label;
};

const ThemeOption kThemeOptions[] = {
    {"system", "跟随系统"},
    {"light", "浅色"},
    {"dark", "深色"},
};

}  // namespace

SettingsDialog::SettingsDialog(const common::AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("SnapOCR 设置"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    // 自建标题栏：去掉系统边框，窗口背景透明以便绘制圆角与投影。
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    buildUi();
    applyStyleSheet();
    loadIntoUi(settings);
}

// ---------------------------------------------------------------------------
// UI 构建
// ---------------------------------------------------------------------------

void SettingsDialog::buildUi()
{
    // 外层留出阴影边距，内层是带圆角的白色“窗口本体”。
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);
    outer->setSpacing(0);

    auto* surface = new QWidget(this);
    surface->setObjectName(QStringLiteral("dialogSurface"));
    surface->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(surface);

    auto* shadow = new QGraphicsDropShadowEffect(surface);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 60));
    surface->setGraphicsEffect(shadow);

    auto* surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(0, 0, 0, 0);
    surfaceLayout->setSpacing(0);

    // ---- 自建标题栏 ----
    m_titleBar = new TitleBar(QStringLiteral("设置"), surface);
    surfaceLayout->addWidget(m_titleBar);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QDialog::reject);

    // ---- 内容区（可滚动，保证小屏/高缩放下也能完整操作） ----
    auto* scroll = new QScrollArea(surface);
    scroll->setObjectName(QStringLiteral("contentScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("contentArea"));
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(20, 14, 20, 6);
    m_contentLayout->setSpacing(14);

    // ---- 热键 ----
    m_captureHotkey = new QKeySequenceEdit(this);
    m_captureHotkey->setMaximumSequenceLength(1);
    m_ocrHotkey = new QKeySequenceEdit(this);
    m_ocrHotkey->setMaximumSequenceLength(1);
    m_contentLayout->addWidget(makeSection(QStringLiteral("热键"), {
        makeRow(QStringLiteral("截图"), m_captureHotkey),
        makeSeparator(),
        makeRow(QStringLiteral("屏幕取字"), m_ocrHotkey),
    }));

    // ---- 保存 ----
    auto* dirRow = new QWidget(this);
    auto* dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(8);
    m_saveDirectory = new QLineEdit(dirRow);
    m_saveDirectory->setPlaceholderText(QStringLiteral("留空则使用 图片\\SnapOCR"));
    auto* browseButton = new QPushButton(QStringLiteral("浏览"), dirRow);
    browseButton->setObjectName(QStringLiteral("secondaryButton"));
    browseButton->setCursor(Qt::PointingHandCursor);
    dirLayout->addWidget(m_saveDirectory, 1);
    dirLayout->addWidget(browseButton);
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

    // JPG 质量：滑块 + 数值标签（比老式自旋框更符合现代工具栏式界面）
    auto* qualityRow = new QWidget(this);
    auto* qualityLayout = new QHBoxLayout(qualityRow);
    qualityLayout->setContentsMargins(0, 0, 0, 0);
    qualityLayout->setSpacing(12);
    m_jpgQuality = new QSlider(Qt::Horizontal, qualityRow);
    m_jpgQuality->setRange(50, 100);
    m_jpgQuality->setSingleStep(1);
    m_jpgQuality->setPageStep(5);
    m_jpgQuality->setMinimumWidth(160);
    m_jpgQualityValue = new QLabel(qualityRow);
    m_jpgQualityValue->setObjectName(QStringLiteral("valueLabel"));
    m_jpgQualityValue->setFixedWidth(44);
    m_jpgQualityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    qualityLayout->addWidget(m_jpgQuality, 1);
    qualityLayout->addWidget(m_jpgQualityValue);
    connect(m_jpgQuality, &QSlider::valueChanged, this, [this](int value) {
        m_jpgQualityValue->setText(QStringLiteral("%1 %").arg(value));
    });
    // PNG 时 JPG 质量无意义：跟随格式启用/禁用。
    connect(m_format, &QComboBox::currentIndexChanged, this, [this]() {
        const bool jpg = m_format->currentData().toString() == QLatin1String("jpg");
        m_jpgQuality->setEnabled(jpg);
        m_jpgQualityValue->setEnabled(jpg);
    });

    m_nameTemplate = new QLineEdit(this);
    m_nameTemplate->setPlaceholderText(QStringLiteral("SnapOCR_yyyyMMdd_HHmmss"));

    m_contentLayout->addWidget(makeSection(QStringLiteral("保存"), {
        makeRow(QStringLiteral("保存目录"), dirRow),
        makeSeparator(),
        makeRow(QStringLiteral("保存格式"), m_format),
        makeSeparator(),
        makeRow(QStringLiteral("JPG 质量"), qualityRow),
        makeSeparator(),
        makeRow(QStringLiteral("文件名模板"), m_nameTemplate),
    }));

    // ---- 外观 ----
    auto* segments = new QWidget(this);
    segments->setObjectName(QStringLiteral("segmentBar"));
    segments->setAttribute(Qt::WA_StyledBackground, true);
    auto* segmentLayout = new QHBoxLayout(segments);
    segmentLayout->setContentsMargins(3, 3, 3, 3);
    segmentLayout->setSpacing(2);
    m_themeGroup = new QButtonGroup(this);
    m_themeGroup->setExclusive(true);
    int themeId = 0;
    for (const ThemeOption& option : kThemeOptions) {
        auto* button = new QToolButton(segments);
        button->setObjectName(QStringLiteral("segmentButton"));
        button->setText(QString::fromUtf8(option.label));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("themeKey", QString::fromLatin1(option.key));
        m_themeGroup->addButton(button, themeId++);
        segmentLayout->addWidget(button);
    }

    m_autoStart = new ToggleSwitch(this);
    m_contentLayout->addWidget(makeSection(QStringLiteral("外观与启动"), {
        makeRow(QStringLiteral("主题"), segments),
        makeSeparator(),
        makeToggleRow(QStringLiteral("开机自动启动"), m_autoStart),
    }));

    // ---- 文字识别 ----
    m_ocrAutoCopy = new ToggleSwitch(this);
    m_contentLayout->addWidget(
        makeSection(QStringLiteral("文字识别"),
                    {makeToggleRow(QStringLiteral("识别后自动复制到剪贴板"), m_ocrAutoCopy)}));

    // ---- 高级 ----
    m_logLevel = new QComboBox(this);
    m_logLevel->addItem(QStringLiteral("错误"), QStringLiteral("error"));
    m_logLevel->addItem(QStringLiteral("警告"), QStringLiteral("warning"));
    m_logLevel->addItem(QStringLiteral("信息"), QStringLiteral("info"));
    m_logLevel->addItem(QStringLiteral("调试"), QStringLiteral("debug"));
    m_contentLayout->addWidget(
        makeSection(QStringLiteral("高级"), {makeRow(QStringLiteral("日志级别"), m_logLevel)}));

    m_contentLayout->addStretch(1);
    m_contentWidget = content;
    scroll->setWidget(content);
    surfaceLayout->addWidget(scroll, 1);

    // ---- 底部 ----
    auto* footer = new QWidget(surface);
    footer->setObjectName(QStringLiteral("footerBar"));
    m_footerWidget = footer;
    footer->setAttribute(Qt::WA_StyledBackground, true);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(20, 12, 20, 16);
    footerLayout->setSpacing(10);

    auto* hint = new QLabel(
        QStringLiteral("热键被其他程序占用时会自动回退并提示，可在此更换。"), footer);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setWordWrap(true);
    footerLayout->addWidget(hint, 1);

    auto* cancelButton = new QPushButton(QStringLiteral("取消"), footer);
    cancelButton->setObjectName(QStringLiteral("secondaryButton"));
    cancelButton->setCursor(Qt::PointingHandCursor);
    auto* okButton = new QPushButton(QStringLiteral("确定"), footer);
    okButton->setObjectName(QStringLiteral("primaryButton"));
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setDefault(true);
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(okButton);
    surfaceLayout->addWidget(footer);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(okButton, &QPushButton::clicked, this, [this]() {
        // 热键合法性校验：无法映射到 Win32 的组合直接拒绝。
        const QKeySequence capture = m_captureHotkey->keySequence();
        const QKeySequence ocr = m_ocrHotkey->keySequence();
        for (const QKeySequence& sequence : {capture, ocr}) {
            if (sequence.isEmpty()) {
                continue;
            }
            QString error;
            if (!app::HotkeyManager::toWin32(sequence, nullptr, nullptr, &error)
                && !error.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("热键不可用"), error);
                return;
            }
        }
        collectFromUi();
        accept();
    });

    setFixedWidth(kDialogWidth + kShadowMargin * 2);
    // 高度在 showEvent 里计算：此时样式表已生效，控件尺寸才是最终值。
    setFixedHeight(600);
}

QWidget* SettingsDialog::makeSeparator()
{
    auto* line = new QFrame(this);
    line->setObjectName(QStringLiteral("rowSeparator"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    return line;
}

QWidget* SettingsDialog::makeRow(const QString& label, QWidget* control)
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 7, 0, 7);
    layout->setSpacing(16);

    auto* nameLabel = new QLabel(label, row);
    nameLabel->setObjectName(QStringLiteral("rowLabel"));
    nameLabel->setMinimumWidth(84);
    layout->addWidget(nameLabel, 0, Qt::AlignVCenter);
    layout->addWidget(control, 1);
    return row;
}

QWidget* SettingsDialog::makeToggleRow(const QString& label, ToggleSwitch* toggle)
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(16);

    auto* nameLabel = new QLabel(label, row);
    nameLabel->setObjectName(QStringLiteral("rowLabel"));
    layout->addWidget(nameLabel, 1, Qt::AlignVCenter);
    layout->addWidget(toggle, 0, Qt::AlignVCenter | Qt::AlignRight);
    return row;
}

QWidget* SettingsDialog::makeSection(const QString& caption, const QVector<QWidget*>& rows)
{
    auto* section = new QWidget(this);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* captionLabel = new QLabel(caption, section);
    captionLabel->setObjectName(QStringLiteral("sectionCaption"));
    layout->addWidget(captionLabel);

    auto* card = new QWidget(section);
    card->setObjectName(QStringLiteral("settingsCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 6, 18, 6);
    cardLayout->setSpacing(0);
    for (QWidget* row : rows) {
        cardLayout->addWidget(row);
    }
    layout->addWidget(card);
    return section;
}

void SettingsDialog::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(R"(
QWidget#dialogSurface {
    background: #F7F8FA;
    border: 1px solid #E1E5EB;
    border-radius: 12px;
}
QWidget#titleBar { background: #FFFFFF; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QLabel#titleBarText { color: #1F2328; font-size: 13px; font-weight: 600; }
QToolButton#titleBarClose {
    border: none; border-radius: 6px; color: #5B6270; font-size: 14px;
}
QToolButton#titleBarClose:hover { background: #E8464A; color: #FFFFFF; }

QLabel#sectionCaption { color: #8A9099; font-size: 12px; font-weight: 600; padding-left: 2px; }
QWidget#settingsCard {
    background: #FFFFFF; border: 1px solid #E5E8ED; border-radius: 10px;
}
QFrame#rowSeparator { background: #F0F2F5; border: none; }
QLabel#rowLabel { color: #1F2328; font-size: 13px; }

QLineEdit, QComboBox, QSpinBox, QKeySequenceEdit {
    background: #FFFFFF; border: 1px solid #D8DCE3; border-radius: 7px;
    padding: 5px 10px; min-height: 22px; color: #1F2328; font-size: 13px;
    selection-background-color: #1F6FEB;
}
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover { border-color: #C2C8D1; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus { border-color: #1F6FEB; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { background: #F4F5F7; color: #9AA0A8; }

QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #FFFFFF; border: 1px solid #D8DCE3; border-radius: 8px;
    padding: 4px; outline: none; selection-background-color: #EAF1FD;
    selection-color: #1F2328;
}

QWidget#segmentBar { background: #EEEFF2; border-radius: 8px; }
QToolButton#segmentButton {
    background: transparent; border: 1px solid transparent; border-radius: 6px;
    padding: 5px 16px; color: #525862; font-size: 13px;
}
QToolButton#segmentButton:hover { color: #1F2328; }
QToolButton#segmentButton:checked {
    background: #FFFFFF; border-color: #D8DCE3; color: #1F2328; font-weight: 600;
}

QPushButton {
    background: #FFFFFF; border: 1px solid #D8DCE3; border-radius: 8px;
    padding: 7px 20px; color: #1F2328; font-size: 13px;
}
QPushButton:hover { background: #F2F4F7; }
QPushButton:pressed { background: #E9ECF1; }
QPushButton#primaryButton {
    background: #1F6FEB; border: none; color: #FFFFFF; font-weight: 600;
}
QPushButton#primaryButton:hover { background: #1A5FD0; }
QPushButton#primaryButton:pressed { background: #174FAE; }

QScrollArea#contentScroll { background: transparent; border: none; }
QWidget#contentArea { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #D2D7DF; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #BFC6D0; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

QLabel#valueLabel { color: #525862; font-size: 13px; }
QSlider::groove:horizontal { height: 4px; background: #E3E7ED; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #1F6FEB; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
    background: #FFFFFF; border: 1px solid #C9CFD8;
}
QSlider::handle:horizontal:hover { border-color: #1F6FEB; }
QSlider::handle:horizontal:disabled { background: #F0F2F5; border-color: #DFE3E9; }
QSlider::sub-page:horizontal:disabled { background: #D8DCE3; }

QWidget#footerBar { background: transparent; }
QLabel#hintLabel { color: #8A9099; font-size: 12px; }
)"));
}

// ---------------------------------------------------------------------------
// 数据绑定
// ---------------------------------------------------------------------------

void SettingsDialog::loadIntoUi(const common::AppSettings& settings)
{
    m_captureHotkey->setKeySequence(QKeySequence(settings.captureHotkey));
    m_ocrHotkey->setKeySequence(QKeySequence(settings.ocrHotkey));
    m_saveDirectory->setText(settings.saveDirectory);
    const int formatIndex = m_format->findData(settings.saveFormat);
    m_format->setCurrentIndex(formatIndex < 0 ? 0 : formatIndex);
    m_jpgQuality->setValue(qBound(50, settings.jpgQuality, 100));
    m_jpgQualityValue->setText(QStringLiteral("%1 %").arg(m_jpgQuality->value()));
    const bool jpgFormat = m_format->currentData().toString() == QLatin1String("jpg");
    m_jpgQuality->setEnabled(jpgFormat);
    m_jpgQualityValue->setEnabled(jpgFormat);
    m_nameTemplate->setText(settings.nameTemplate);

    bool themeMatched = false;
    for (QAbstractButton* button : m_themeGroup->buttons()) {
        if (button->property("themeKey").toString() == settings.theme) {
            button->setChecked(true);
            themeMatched = true;
            break;
        }
    }
    if (!themeMatched && !m_themeGroup->buttons().isEmpty()) {
        m_themeGroup->buttons().first()->setChecked(true);
    }

    m_autoStart->setChecked(settings.startWithWindows);
    m_autoStart->setKnobPosition(settings.startWithWindows ? 1.0 : 0.0);
    m_ocrAutoCopy->setChecked(settings.ocrAutoCopy);
    m_ocrAutoCopy->setKnobPosition(settings.ocrAutoCopy ? 1.0 : 0.0);

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

    m_settings.theme = QStringLiteral("system");
    if (QAbstractButton* checked = m_themeGroup->checkedButton()) {
        m_settings.theme = checked->property("themeKey").toString();
    }

    m_settings.startWithWindows = m_autoStart->isChecked();
    m_settings.ocrAutoCopy = m_ocrAutoCopy->isChecked();
    m_settings.logLevel = m_logLevel->currentData().toString();
}

common::AppSettings SettingsDialog::settings() const
{
    return m_settings;
}

void SettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // 首次显示时按内容真实高度定型（样式生效后量才准），并限制在屏幕 85% 高度内。
    if (!m_heightAdjusted && m_titleBar != nullptr && m_contentWidget != nullptr
        && m_footerWidget != nullptr) {
        const QScreen* screen = QGuiApplication::primaryScreen();
        const int availableHeight = screen != nullptr ? screen->availableGeometry().height() : 900;
        const int chrome = kShadowMargin * 2 + m_titleBar->sizeHint().height()
                           + m_footerWidget->sizeHint().height();
        const int maxHeight = static_cast<int>(availableHeight * 0.85) + kShadowMargin * 2;
        // +16 留出滚动区与内容之间的余量，避免差几像素出现滚动条。
        const int naturalHeight = chrome + m_contentWidget->sizeHint().height() + 16;
        setFixedHeight(qMin(naturalHeight, maxHeight));
        m_heightAdjusted = true;
    }

    // 首次显示时把窗口夹取到所在屏幕的可用区域内：
    // 无边框窗口不会自动避开任务栏与屏幕边缘。
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

}  // namespace snapocr::ui
