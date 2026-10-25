#pragma once

#include <QString>
#include <QStringList>

class QApplication;

namespace snapocr::i18n {

// 界面翻译骨架（阶段三 §8.3）。
//
// 说明：中文是源语言（代码里直接写中文），英文等语言通过 Qt Linguist 的
// .ts/.qm 提供。加载顺序：exe 同目录 translations/ → 内嵌资源 :/i18n/。
// 找不到对应翻译时保持源语言（不报错），所以缺翻译不影响使用。
//
// locale 形如 "en-US"、"en"、"zh-CN"；中文直接返回 false（不安装翻译器）。
bool installUiTranslator(QApplication* app, const QString& locale);

// 当前生效的界面语言；空字符串表示源语言（中文）。
QString currentUiLanguage();

// 可用的翻译语言（按 exe 同目录 translations/*.qm + 资源扫描）。
QStringList availableUiLanguages();

}  // namespace snapocr::i18n
