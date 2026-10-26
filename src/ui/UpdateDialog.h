#pragma once

#include <QDialog>

#include "update/UpdateManifest.h"

class QLabel;
class QPlainTextEdit;

namespace snapocr::ui {

// 更新可用对话框（F-SYS-16）：展示版本号、体积、更新说明，
// 让用户选择「下载并安装 / 稍后 / 跳过此版本」。
class UpdateDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Choice
    {
        RemindLater,
        DownloadAndInstall,
        SkipVersion
    };

    UpdateDialog(const snapocr::update::UpdateRelease& release, const QString& currentVersion,
                 QWidget* parent = nullptr);

    Choice choice() const { return m_choice; }

private:
    void applyStyleSheet();

    Choice m_choice = Choice::RemindLater;
    QPlainTextEdit* m_notes = nullptr;
    QLabel* m_sizeLabel = nullptr;
};

}  // namespace snapocr::ui
