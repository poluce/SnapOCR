#include "ui/HistoryDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/ImageDrag.h"
#include "ui/Theme.h"

namespace snapocr::ui {
namespace {

// 缩略图列表：拖到资源管理器/IM 时给出文件 URL（F-SYS-12）
class HistoryListWidget final : public QListWidget
{
public:
    using QListWidget::QListWidget;

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override
    {
        if (items.isEmpty()) {
            return nullptr;
        }
        const QString path = items.first()->data(Qt::UserRole).toString();
        return ui::fileMimeData(path);
    }
};

constexpr int kThumbWidth = 168;
constexpr int kThumbHeight = 104;
constexpr int kGridWidth = 190;
constexpr int kGridHeight = 148;

}  // namespace

HistoryDialog::HistoryDialog(history::HistoryStore* store, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
{
    setWindowTitle(tr("截图历史"));
    setModal(false);
    resize(760, 520);

    // 与主窗口/设置面板一致的浅色风格
    applyStyleSheet();
    connect(&ui::ThemeNotifier::instance(), &ui::ThemeNotifier::themeChanged, this,
            &HistoryDialog::applyStyleSheet);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->setSpacing(10);

    auto* header = new QLabel(tr("最近 50 张截图（双击复制，可直接贴图）"), this);
    header->setObjectName(QStringLiteral("historyHeader"));
    layout->addWidget(header);

    m_list = new HistoryListWidget(this);
    m_list->setObjectName(QStringLiteral("historyList"));
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kThumbWidth, kThumbHeight));
    m_list->setGridSize(QSize(kGridWidth, kGridHeight));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(6);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setDefaultDropAction(Qt::CopyAction);
    layout->addWidget(m_list, 1);

    m_emptyHint = new QLabel(tr("还没有历史截图：完成一次截图（复制/保存/贴图）后就会出现在这里"), this);
    m_emptyHint->setObjectName(QStringLiteral("historyEmptyHint"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyHint);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    m_copyButton = new QPushButton(tr("复制"), this);
    m_pinButton = new QPushButton(tr("贴图"), this);
    m_saveButton = new QPushButton(tr("另存为…"), this);
    m_deleteButton = new QPushButton(tr("删除"), this);
    buttons->addWidget(m_copyButton);
    buttons->addWidget(m_pinButton);
    buttons->addWidget(m_saveButton);
    buttons->addStretch(1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("historyStatus"));
    buttons->addWidget(m_status);
    buttons->addWidget(m_deleteButton);

    auto* closeButton = new QPushButton(tr("关闭"), this);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::currentItemChanged, this, [this]() { updateButtons(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this]() {
        const QImage image = currentImage();
        if (!image.isNull()) {
            emit copyRequested(image);
            m_status->setText(tr("已复制"));
        }
    });
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        const QImage image = currentImage();
        if (!image.isNull()) {
            emit copyRequested(image);
            m_status->setText(tr("已复制"));
        }
    });
    connect(m_pinButton, &QPushButton::clicked, this, [this]() {
        const QImage image = currentImage();
        if (!image.isNull()) {
            emit pinRequested(image);
            m_status->setText(tr("已贴图"));
        }
    });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        const QImage image = currentImage();
        if (!image.isNull()) {
            emit saveRequested(image);
        }
    });
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        const history::HistoryEntry entry = currentEntry();
        if (!entry.id.isEmpty() && m_store != nullptr) {
            m_store->remove(entry.id);
            m_status->setText(tr("已删除"));
        }
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    if (m_store != nullptr) {
        // 删除/新增后自动刷新，避免界面与磁盘不一致
        connect(m_store, &history::HistoryStore::changed, this, &HistoryDialog::refresh);
    }

    refresh();
}


void HistoryDialog::refresh()
{
    const QVector<history::HistoryEntry> entries =
        m_store != nullptr ? m_store->entries() : QVector<history::HistoryEntry>{};

    m_list->clear();
    for (const history::HistoryEntry& entry : entries) {
        const QImage image = m_store->load(entry);
        const QPixmap thumb = QPixmap::fromImage(image).scaled(
            kThumbWidth, kThumbHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        auto* item = new QListWidgetItem(QIcon(thumb),
                                         QStringLiteral("%1\n%2 × %3")
                                             .arg(entry.capturedAt.toString(QStringLiteral("MM-dd HH:mm:ss")))
                                             .arg(entry.size.width())
                                             .arg(entry.size.height()));
        item->setData(Qt::UserRole, entry.filePath);
        item->setData(Qt::UserRole + 1, entry.id);
        item->setToolTip(entry.filePath);
        m_list->addItem(item);
    }

    const bool empty = entries.isEmpty();
    m_emptyHint->setVisible(empty);
    m_list->setVisible(!empty);
    if (!empty) {
        m_list->setCurrentRow(0);
    }
    updateButtons();
}

history::HistoryEntry HistoryDialog::currentEntry() const
{
    history::HistoryEntry entry;
    const QListWidgetItem* item = m_list->currentItem();
    if (item == nullptr || m_store == nullptr) {
        return entry;
    }
    const QString id = item->data(Qt::UserRole + 1).toString();
    for (const history::HistoryEntry& candidate : m_store->entries()) {
        if (candidate.id == id) {
            return candidate;
        }
    }
    return entry;
}

QImage HistoryDialog::currentImage() const
{
    if (m_store == nullptr) {
        return {};
    }
    const history::HistoryEntry entry = currentEntry();
    return entry.id.isEmpty() ? QImage() : m_store->load(entry);
}

void HistoryDialog::updateButtons()
{
    const bool hasSelection = m_list->currentItem() != nullptr;
    m_copyButton->setEnabled(hasSelection);
    m_pinButton->setEnabled(hasSelection);
    m_saveButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
    if (hasSelection && m_status->text().isEmpty()) {
        m_status->clear();
    }
}


void HistoryDialog::applyStyleSheet()
{
    setStyleSheet(ui::styled(QStringLiteral(R"(
        QDialog { background: {window}; }
        QLabel#historyHeader { color: #5A616B; font-size: 12px; }
        QLabel#historyEmptyHint { color: {textFaint}; font-size: 13px; padding: 24px; }
        QLabel#historyStatus { color: {accent}; font-size: 12px; }
        QListWidget#historyList {
            background: {surface}; border: 1px solid {border}; border-radius: 10px; padding: 8px;
            outline: none;
        }
        QListWidget#historyList::item {
            color: #2B2F36; border-radius: 8px; padding: 6px;
        }
        QListWidget#historyList::item:selected {
            background: {accentSoft}; color: {accent};
        }
        QPushButton {
            background: {surface}; border: 1px solid {border}; border-radius: 8px;
            padding: 6px 14px; color: #2B2F36; font-size: 13px;
        }
        QPushButton:hover { border-color: {accent}; color: {accent}; }
        QPushButton:pressed { background: {accentSoft}; }
        QPushButton:disabled { color: {textFaint}; border-color: {border}; }
    )")));
}

}  // namespace snapocr::ui
