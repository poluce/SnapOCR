#include <QtTest>

#include <QApplication>
#include <QCoreApplication>

#include "i18n/Translator.h"
#include "ui/MainWindow.h"

using namespace snapocr;

// 界面翻译骨架（阶段三 §8.3）：
// 中文是源语言；英文通过 translations/snapocr_en.qm 加载。
class TranslatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void chineseIsSourceLanguage();
    void loadsEnglishTranslation();
    void missingLanguageFallsBackToSource();
    void availableLanguagesIncludeEnglish();
};

void TranslatorTest::cleanup()
{
    i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                              QStringLiteral("zh-CN"));
}

void TranslatorTest::chineseIsSourceLanguage()
{
    QVERIFY(!i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                                      QStringLiteral("zh-CN")));
    QVERIFY(i18n::currentUiLanguage().isEmpty());
    // 源语言下 Qt 直接返回原文
    QCOMPARE(QCoreApplication::translate("MainWindow", "截图"), QStringLiteral("截图"));
}

void TranslatorTest::loadsEnglishTranslation()
{
    const bool loaded = i18n::installUiTranslator(
        qobject_cast<QApplication*>(QApplication::instance()), QStringLiteral("en-US"));
    QVERIFY2(loaded, "未找到 translations/snapocr_en.qm（检查构建是否执行了 lrelease）");
    QVERIFY(!i18n::currentUiLanguage().isEmpty());

    QCOMPARE(QCoreApplication::translate("MainWindow", "截图"), QStringLiteral("Capture"));
    QCOMPARE(QCoreApplication::translate("TitleBar", "关闭"), QStringLiteral("Close"));
}

void TranslatorTest::missingLanguageFallsBackToSource()
{
    QVERIFY(!i18n::installUiTranslator(qobject_cast<QApplication*>(QApplication::instance()),
                                      QStringLiteral("fr-FR")));
    QCOMPARE(QCoreApplication::translate("MainWindow", "截图"), QStringLiteral("截图"));
}

void TranslatorTest::availableLanguagesIncludeEnglish()
{
    const QStringList languages = i18n::availableUiLanguages();
    QVERIFY2(languages.contains(QStringLiteral("en")), qPrintable(languages.join(',')));
}

QTEST_MAIN(TranslatorTest)

#include "test_translator.moc"
