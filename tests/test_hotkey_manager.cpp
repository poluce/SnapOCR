#include <QtTest>

#include <windows.h>

#include "app/HotkeyManager.h"

using snapocr::app::HotkeyManager;

class HotkeyManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void mapsFunctionKeys();
    void mapsLettersAndModifiers();
    void mapsDigits();
    void rejectsUnsupportedKeys();
    void rejectsEmptySequence();
};

void HotkeyManagerTest::mapsFunctionKeys()
{
    unsigned modifiers = 0;
    unsigned virtualKey = 0;
    QString error;
    QVERIFY(HotkeyManager::toWin32(QKeySequence(QStringLiteral("F1")), &modifiers, &virtualKey,
                                   &error));
    QCOMPARE(virtualKey, static_cast<unsigned>(VK_F1));
    QCOMPARE(modifiers, 0u);

    QVERIFY(HotkeyManager::toWin32(QKeySequence(QStringLiteral("F12")), &modifiers, &virtualKey,
                                   &error));
    QCOMPARE(virtualKey, static_cast<unsigned>(VK_F1) + 11u);
}

void HotkeyManagerTest::mapsLettersAndModifiers()
{
    unsigned modifiers = 0;
    unsigned virtualKey = 0;
    QString error;
    QVERIFY(HotkeyManager::toWin32(QKeySequence(QStringLiteral("Ctrl+Shift+A")), &modifiers,
                                   &virtualKey, &error));
    QCOMPARE(virtualKey, static_cast<unsigned>('A'));
    QCOMPARE(modifiers, static_cast<unsigned>(MOD_CONTROL | MOD_SHIFT));

    QVERIFY(HotkeyManager::toWin32(QKeySequence(QStringLiteral("Alt+Z")), &modifiers, &virtualKey,
                                   &error));
    QCOMPARE(virtualKey, static_cast<unsigned>('Z'));
    QCOMPARE(modifiers, static_cast<unsigned>(MOD_ALT));

    QVERIFY(HotkeyManager::toWin32(QKeySequence(QStringLiteral("Meta+B")), &modifiers, &virtualKey,
                                   &error));
    QCOMPARE(modifiers, static_cast<unsigned>(MOD_WIN));
}

void HotkeyManagerTest::mapsDigits()
{
    unsigned modifiers = 0;
    unsigned virtualKey = 0;
    QString error;
    QVERIFY(HotkeyManager::toWin32(QKeySequence(QStringLiteral("Ctrl+7")), &modifiers, &virtualKey,
                                   &error));
    QCOMPARE(virtualKey, static_cast<unsigned>('7'));
}

void HotkeyManagerTest::rejectsUnsupportedKeys()
{
    unsigned modifiers = 0;
    unsigned virtualKey = 0;
    QString error;
    // F13 之外的媒体键等不在支持范围内。
    const bool ok = HotkeyManager::toWin32(QKeySequence(Qt::Key_MediaPlay), &modifiers, &virtualKey,
                                           &error);
    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
}

void HotkeyManagerTest::rejectsEmptySequence()
{
    unsigned modifiers = 0;
    unsigned virtualKey = 0;
    QString error;
    QVERIFY(!HotkeyManager::toWin32(QKeySequence(), &modifiers, &virtualKey, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(HotkeyManagerTest)

#include "test_hotkey_manager.moc"
