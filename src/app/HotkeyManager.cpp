#include "app/HotkeyManager.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>

#include <windows.h>

namespace snapocr::app {
namespace {

// Qt 键值 → Win32 虚拟键。
bool mapVirtualKey(Qt::Key key, unsigned* virtualKey)
{
    if (virtualKey == nullptr) {
        return false;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        *virtualKey = VK_F1 + static_cast<unsigned>(key - Qt::Key_F1);
        return true;
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        *virtualKey = static_cast<unsigned>('A' + (key - Qt::Key_A));
        return true;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        *virtualKey = static_cast<unsigned>('0' + (key - Qt::Key_0));
        return true;
    }
    switch (key) {
    case Qt::Key_Print:
        *virtualKey = VK_SNAPSHOT;
        return true;
    case Qt::Key_Pause:
        *virtualKey = VK_PAUSE;
        return true;
    case Qt::Key_Insert:
        *virtualKey = VK_INSERT;
        return true;
    case Qt::Key_Delete:
        *virtualKey = VK_DELETE;
        return true;
    case Qt::Key_Home:
        *virtualKey = VK_HOME;
        return true;
    case Qt::Key_End:
        *virtualKey = VK_END;
        return true;
    case Qt::Key_PageUp:
        *virtualKey = VK_PRIOR;
        return true;
    case Qt::Key_PageDown:
        *virtualKey = VK_NEXT;
        return true;
    case Qt::Key_Space:
        *virtualKey = VK_SPACE;
        return true;
    case Qt::Key_Tab:
        *virtualKey = VK_TAB;
        return true;
    case Qt::Key_Escape:
        *virtualKey = VK_ESCAPE;
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        *virtualKey = VK_RETURN;
        return true;
    default:
        break;
    }
    return false;
}

}  // namespace

struct HotkeyManager::Private
{
    QHash<int, Action> registeredActions;
    QHash<int, QString> registeredTexts;
};

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

HotkeyManager::~HotkeyManager()
{
    unregisterAll();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
    delete d;
    d = nullptr;
}

bool HotkeyManager::toWin32(const QKeySequence& sequence, unsigned* modifiers, unsigned* virtualKey,
                            QString* errorMessage)
{
    if (modifiers == nullptr || virtualKey == nullptr) {
        return false;
    }
    if (sequence.isEmpty() || sequence.count() < 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("热键为空");
        }
        return false;
    }

    const QKeyCombination combination = sequence[0];
    unsigned mods = 0;
    const Qt::KeyboardModifiers qtMods = combination.keyboardModifiers();
    if (qtMods.testFlag(Qt::ControlModifier)) {
        mods |= MOD_CONTROL;
    }
    if (qtMods.testFlag(Qt::AltModifier)) {
        mods |= MOD_ALT;
    }
    if (qtMods.testFlag(Qt::ShiftModifier)) {
        mods |= MOD_SHIFT;
    }
    if (qtMods.testFlag(Qt::MetaModifier)) {
        mods |= MOD_WIN;
    }

    unsigned vk = 0;
    if (!mapVirtualKey(combination.key(), &vk)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("不支持的快捷键：%1")
                                .arg(sequence.toString(QKeySequence::NativeText));
        }
        return false;
    }

    *modifiers = mods;
    *virtualKey = vk;
    return true;
}

bool HotkeyManager::registerHotkey(Action action, const QString& sequenceText, QString* errorMessage)
{
    return registerHotkey(action, QKeySequence(sequenceText), errorMessage);
}

bool HotkeyManager::registerHotkey(Action action, const QKeySequence& sequence, QString* errorMessage)
{
    unsigned mods = 0;
    unsigned vk = 0;
    if (!toWin32(sequence, &mods, &vk, errorMessage)) {
        return false;
    }

    unregisterHotkey(action);

    const int id = static_cast<int>(action);
    if (::RegisterHotKey(nullptr, id, mods | MOD_NOREPEAT, vk) == FALSE) {
        const DWORD code = ::GetLastError();
        if (errorMessage != nullptr) {
            *errorMessage = code == ERROR_HOTKEY_ALREADY_REGISTERED
                                ? QStringLiteral("热键 %1 已被其他程序占用")
                                      .arg(sequence.toString(QKeySequence::NativeText))
                                : QStringLiteral("注册热键 %1 失败（错误码 %2）")
                                      .arg(sequence.toString(QKeySequence::NativeText))
                                      .arg(code);
        }
        return false;
    }

    d->registeredActions.insert(id, action);
    d->registeredTexts.insert(id, sequence.toString(QKeySequence::NativeText));
    return true;
}

void HotkeyManager::unregisterHotkey(Action action)
{
    const int id = static_cast<int>(action);
    if (!d->registeredActions.contains(id)) {
        return;
    }
    ::UnregisterHotKey(nullptr, id);
    d->registeredActions.remove(id);
    d->registeredTexts.remove(id);
}

void HotkeyManager::unregisterAll()
{
    for (auto it = d->registeredActions.constBegin(); it != d->registeredActions.constEnd(); ++it) {
        ::UnregisterHotKey(nullptr, it.key());
    }
    d->registeredActions.clear();
    d->registeredTexts.clear();
}

QString HotkeyManager::registeredText(Action action) const
{
    return d->registeredTexts.value(static_cast<int>(action));
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result);
    if (eventType != QByteArrayLiteral("windows_generic_MSG")
        && eventType != QByteArrayLiteral("windows_dispatcher_MSG")) {
        return false;
    }
    auto* msg = static_cast<MSG*>(message);
    if (msg == nullptr || msg->message != WM_HOTKEY) {
        return false;
    }
    handleHotkey(static_cast<int>(msg->wParam));
    return true;
}

void HotkeyManager::handleHotkey(int id)
{
    const Action action = d->registeredActions.value(id, Action::Capture);
    switch (action) {
    case Action::Capture:
        emit captureRequested();
        break;
    case Action::Ocr:
        emit ocrRequested();
        break;
    case Action::OverlayCancel:
        emit overlayCancelRequested();
        break;
    case Action::OverlayConfirm:
        emit overlayConfirmRequested();
        break;
    }
}

}  // namespace snapocr::app
