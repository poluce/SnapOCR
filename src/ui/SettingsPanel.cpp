#include "ui/SettingsPanel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/ToggleSwitch.h"

namespace snapocr::ui {
namespace {

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

SettingsPanel::SettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyStyleSheet();
}

void SettingsPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    // ---- 热键 ----
    m_captureHotkey = new QKeySequenceEdit(this);
    m_captureHotkey->setMaximumSequenceLength(1);
    m_ocrHotkey = new QKeySequenceEdit(this);
    m_ocrHotkey->setMaximumSequenceLength(1);
    // 热键要等用户录完再应用，避免每按一个键就重注册。
    connect(m_captureHotkey, &QKeySequenceEdit::editingFinished, this, &SettingsPanel::emitChanged);
    connect(m_ocrHotkey, &QKeySequenceEdit::editingFinished, this, &SettingsPanel::emitChanged);

    root->addWidget(makeSection(QStringLiteral("热键"), {
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
            emitChanged();
        }
    });
    connect(m_saveDirectory, &QLineEdit::editingFinished, this, &SettingsPanel::emitChanged);

    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG（无损）"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPG（体积小）"), QStringLiteral("jpg"));

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
        emitChanged();
    });
    connect(m_format, &QComboBox::currentIndexChanged, this, [this]() {
        const bool jpg = m_format->currentData().toString() == QLatin1String("jpg");
        m_jpgQuality->setEnabled(jpg);
        m_jpgQualityValue->setEnabled(jpg);
        emitChanged();
    });

    m_nameTemplate = new QLineEdit(this);
    m_nameTemplate->setPlaceholderText(QStringLiteral("SnapOCR_yyyyMMdd_HHmmss"));
    connect(m_nameTemplate, &QLineEdit::editingFinished, this, &SettingsPanel::emitChanged);

    root->addWidget(makeSection(QStringLiteral("保存"), {
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
    connect(m_themeGroup, &QButtonGroup::idClicked, this, [this](int) { emitChanged(); });

    m_autoStart = new ToggleSwitch(this);
    connect(m_autoStart, &QAbstractButton::toggled, this, &SettingsPanel::emitChanged);

    root->addWidget(makeSection(QStringLiteral("外观与启动"), {
        makeRow(QStringLiteral("主题"), segments),
        makeSeparator(),
        makeToggleRow(QStringLiteral("开机自动启动"), m_autoStart),
    }));

    // ---- 文字识别 ----
    m_ocrAutoCopy = new ToggleSwitch(this);
    connect(m_ocrAutoCopy, &QAbstractButton::toggled, this, &SettingsPanel::emitChanged);
    root->addWidget(
        makeSection(QStringLiteral("文字识别"),
                    {makeToggleRow(QStringLiteral("识别后自动复制到剪贴板"), m_ocrAutoCopy)}));

    // ---- 高级 ----
    m_logLevel = new QComboBox(this);
    m_logLevel->addItem(QStringLiteral("错误"), QStringLiteral("error"));
    m_logLevel->addItem(QStringLiteral("警告"), QStringLiteral("warning"));
    m_logLevel->addItem(QStringLiteral("信息"), QStringLiteral("info"));
    m_logLevel->addItem(QStringLiteral("调试"), QStringLiteral("debug"));
    connect(m_logLevel, &QComboBox::currentIndexChanged, this, &SettingsPanel::emitChanged);
    root->addWidget(
        makeSection(QStringLiteral("高级"), {makeRow(QStringLiteral("日志级别"), m_logLevel)}));
}

QWidget* SettingsPanel::makeSeparator()
{
    auto* line = new QFrame(this);
    line->setObjectName(QStringLiteral("rowSeparator"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    return line;
}

QWidget* SettingsPanel::makeRow(const QString& label, QWidget* control)
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

QWidget* SettingsPanel::makeToggleRow(const QString& label, ToggleSwitch* toggle)
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

QWidget* SettingsPanel::makeSection(const QString& caption, const QVector<QWidget*>& rows)
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

void SettingsPanel::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(R"(
QLabel#sectionCaption { color: #8A9099; font-size: 12px; font-weight: 600; padding-left: 2px; }
QWidget#settingsCard { background: #FFFFFF; border: 1px solid #E5E8ED; border-radius: 10px; }
QFrame#rowSeparator { background: #F0F2F5; border: none; }
QLabel#rowLabel { color: #1F2328; font-size: 13px; }
QLabel#valueLabel { color: #525862; font-size: 13px; }

QLineEdit, QComboBox, QKeySequenceEdit {
    background: #FFFFFF; border: 1px solid #D8DCE3; border-radius: 7px;
    padding: 5px 10px; min-height: 22px; color: #1F2328; font-size: 13px;
    selection-background-color: #1F6FEB;
}
QLineEdit:hover, QComboBox:hover, QKeySequenceEdit:hover { border-color: #C2C8D1; }
QLineEdit:focus, QComboBox:focus, QKeySequenceEdit:focus { border-color: #1F6FEB; }
QLineEdit:disabled, QComboBox:disabled { background: #F4F5F7; color: #9AA0A8; }

QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #FFFFFF; border: 1px solid #D8DCE3; border-radius: 8px;
    padding: 4px; outline: none; selection-background-color: #EAF1FD; selection-color: #1F2328;
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

QSlider::groove:horizontal { height: 4px; background: #E3E7ED; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #1F6FEB; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
    background: #FFFFFF; border: 1px solid #C9CFD8;
}
QSlider::handle:horizontal:hover { border-color: #1F6FEB; }
QSlider::handle:horizontal:disabled { background: #F0F2F5; border-color: #DFE3E9; }
QSlider::sub-page:horizontal:disabled { background: #D8DCE3; }
)"));
}

void SettingsPanel::setSettings(const common::AppSettings& settings)
{
    m_loading = true;
    m_captureHotkey->setKeySequence(QKeySequence(settings.captureHotkey));
    m_ocrHotkey->setKeySequence(QKeySequence(settings.ocrHotkey));
    m_saveDirectory->setText(settings.saveDirectory);
    const int formatIndex = m_format->findData(settings.saveFormat);
    m_format->setCurrentIndex(formatIndex < 0 ? 0 : formatIndex);
    m_jpgQuality->setValue(qBound(50, settings.jpgQuality, 100));
    m_jpgQualityValue->setText(QStringLiteral("%1 %").arg(m_jpgQuality->value()));
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
    m_loading = false;
}

common::AppSettings SettingsPanel::settings() const
{
    common::AppSettings result;
    const QKeySequence capture = m_captureHotkey->keySequence();
    const QKeySequence ocr = m_ocrHotkey->keySequence();
    result.captureHotkey =
        capture.isEmpty() ? QStringLiteral("F1") : capture.toString(QKeySequence::PortableText);
    result.ocrHotkey =
        ocr.isEmpty() ? QStringLiteral("F4") : ocr.toString(QKeySequence::PortableText);
    result.saveDirectory = m_saveDirectory->text().trimmed();
    result.saveFormat = m_format->currentData().toString();
    result.jpgQuality = m_jpgQuality->value();
    result.nameTemplate = m_nameTemplate->text().trimmed();
    result.theme = QStringLiteral("system");
    if (QAbstractButton* checked = m_themeGroup->checkedButton()) {
        result.theme = checked->property("themeKey").toString();
    }
    result.startWithWindows = m_autoStart->isChecked();
    result.ocrAutoCopy = m_ocrAutoCopy->isChecked();
    result.logLevel = m_logLevel->currentData().toString();
    return result;
}

void SettingsPanel::emitChanged()
{
    if (m_loading) {
        return;  // 加载配置时不要回抛变更，避免上层重复应用
    }
    emit settingsChanged(settings());
}

}  // namespace snapocr::ui
