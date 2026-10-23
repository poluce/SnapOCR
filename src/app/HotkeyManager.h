#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>
#include <QString>

namespace snapocr::app {

// 全局热键管理：使用 Win32 RegisterHotKey（线程级，不安装键盘钩子）。
// 通过 QAbstractNativeEventFilter 捕获 WM_HOTKEY，注册失败经返回值 + errorMessage 告知，
// 不得静默失效（热键被占用是常见情况）。
class HotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    enum class Action
    {
        Capture = 1,
        Ocr = 2
    };

    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    bool registerHotkey(Action action, const QKeySequence& sequence, QString* errorMessage);
    bool registerHotkey(Action action, const QString& sequenceText, QString* errorMessage);

    void unregisterHotkey(Action action);
    void unregisterAll();

    QString registeredText(Action action) const;

    // 组合键 → Win32 修饰符/虚拟键；失败返回 false（供设置页校验复用）。
    static bool toWin32(const QKeySequence& sequence, unsigned* modifiers, unsigned* virtualKey,
                        QString* errorMessage);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void captureRequested();
    void ocrRequested();

private:
    void handleHotkey(int id);

    struct Private;
    Private* d = nullptr;
};

}  // namespace snapocr::app
