#pragma once

#include <QDialog>
#include <QImage>
#include <QVector>

#include "history/HistoryStore.h"

class QLabel;
class QListWidget;
class QMimeData;
class QPushButton;

namespace snapocr::ui {

// 截图历史（F-CAP-17）：缩略图网格 + 复制/贴图/另存为/删除。
class HistoryDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(history::HistoryStore* store, QWidget* parent = nullptr);

    void refresh();

signals:
    void copyRequested(const QImage& image);
    void pinRequested(const QImage& image);
    void saveRequested(const QImage& image);

private:
    void applyStyleSheet();
    history::HistoryEntry currentEntry() const;
    QImage currentImage() const;
    void updateButtons();

    history::HistoryStore* m_store = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_emptyHint = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_pinButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

}  // namespace snapocr::ui
