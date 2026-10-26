#!/usr/bin/env python3
"""把 lupdate 生成的 .ts 规范化为「带命名空间的 context + 英文翻译」。

背景：`lupdate` 在缺 Qt 头/命名空间解析时会退化成无 context 的 .ts，
而 Qt `tr()` 的运行时 context 是带命名空间的类名（如
`snapocr::ui::SettingsPanel`），context 不对翻译就查不到。
本项目固定做法：
  1) lupdate 生成带 location 的 .ts：
     lupdate -no-obsolete <sources...> -ts translations/snapocr_en.ts
  2) 运行本脚本：按源文件映射 context，并补上英文（TR_EN 字典）
  3) 构建期 lrelease 编译成 .qm

用法：python3 scripts/gen_translations.py translations/snapocr_en.ts
"""

import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

CONTEXT_BY_FILE = {
    'SettingsPanel.cpp': 'snapocr::ui::SettingsPanel',
    'HistoryDialog.cpp': 'snapocr::ui::HistoryDialog',
    'OcrResultDialog.cpp': 'snapocr::ui::OcrResultDialog',
    'ScrollingBar.cpp': 'snapocr::ui::ScrollingBar',
    'PinWindow.cpp': 'snapocr::ui::PinWindow',
    'MainWindow.cpp': 'snapocr::ui::MainWindow',
    'TitleBar.cpp': 'snapocr::ui::TitleBar',
    'TrayController.cpp': 'snapocr::app::TrayController',
    'Application.cpp': 'snapocr::app::Application',
}

# 同一中文在不同 context 下含义不同的，按 (context, source) 覆盖。
CONTEXT_OVERRIDES = {
    # 设置面板的「关闭」是"延时关闭/不延时"，不是对话框的 Close
    ('snapocr::ui::SettingsPanel', '关闭'): 'Off',
}

TR_EN = {
    # MainWindow / Tray
    '截图': 'Capture', '屏幕取字': 'Grab Text', '截图历史': 'History', '就绪': 'Ready',
    '贴图最近一张': 'Pin Latest', '打开主窗口': 'Open Main Window', '设置': 'Settings',
    '退出': 'Quit', '截图\t%1': 'Capture\t%1', '屏幕取字\t%1': 'Grab Text\t%1',
    # HistoryDialog
    '最近 50 张截图（双击复制，可直接贴图）':
        'Last 50 screenshots (double-click to copy, drag out or pin)',
    '还没有历史截图：完成一次截图（复制/保存/贴图）后就会出现在这里':
        'No screenshots yet. Finish a capture (copy/save/pin) and it will show up here.',
    '复制': 'Copy', '贴图': 'Pin', '另存为…': 'Save As…', '删除': 'Delete', '关闭': 'Close',
    '已复制': 'Copied', '已贴图': 'Pinned', '已删除': 'Deleted',
    # OcrResultDialog
    '提取文字': 'Grab Text', '识别结果会显示在这里': 'Recognized text will appear here.',
    '合并段落（清理排版）': 'Merge paragraphs (clean layout)',
    '把被排版拆断的行合并成段落；取消勾选可保留引擎原始换行':
        'Merge lines split by layout into paragraphs; uncheck to keep raw line breaks.',
    '重新识别': 'Retry', '已复制到剪贴板': 'Copied to clipboard',
    '识别失败：%1': 'Recognition failed: %1',
    '未识别到文字，可尝试重新框选或改用更大的区域':
        'No text recognized. Try selecting a larger area.',
    '语言：%1 · 耗时：%2ms · %3 行': 'Language: %1 · Time: %2ms · %3 lines',
    '系统默认': 'System default', ' · 已合并段落': ' · paragraphs merged',
    # PinWindow
    '提取文字 (Ctrl+T)': 'Grab Text (Ctrl+T)',
    '在编辑器中打开 (Ctrl+E)': 'Open in Editor (Ctrl+E)',
    '复制到剪贴板 (Ctrl+C)': 'Copy to Clipboard (Ctrl+C)',
    '保存 (Ctrl+S)': 'Save (Ctrl+S)',
    '放大': 'Zoom In', '缩小': 'Zoom Out', '旋转 90° (R)': 'Rotate 90° (R)',
    '恢复 100%': 'Reset to 100%', '关闭 (Esc)': 'Close (Esc)',
    # ScrollingBar
    '滚动页面，SnapOCR 会自动拼接；完成后点「完成」':
        'Scroll the page; SnapOCR stitches automatically. Click Finish when done.',
    '已抓取 0 帧': '0 frames captured', '取消': 'Cancel', '完成': 'Finish',
    '已抓取 %1 帧 · 约 %2 px': '%1 frames · ~%2 px',
    # SettingsPanel
    '简体中文': 'Simplified Chinese',
    '界面语言切换后需要重启 SnapOCR 生效':
        'Restart SnapOCR for the language change to take effect.',
    '界面语言': 'Language', '1 秒': '1 s', '2 秒': '2 s', '3 秒': '3 s', '5 秒': '5 s',
    '10 秒': '10 s', '截图热键': 'Capture hotkey', '取字热键': 'OCR hotkey',
    '延时截图': 'Capture delay', '捕获鼠标光标': 'Include mouse cursor',
    '窗口识别': 'Window detection', '留空则使用 图片\\SnapOCR': 'Empty = Pictures\\SnapOCR',
    '浏览': 'Browse', '选择保存目录': 'Choose save folder', 'PNG（无损）': 'PNG (lossless)',
    'JPG（体积小）': 'JPG (smaller)', '保存目录': 'Save folder', '保存格式': 'Format',
    'JPG 质量': 'JPG quality', '文件名模板': 'File name template', '主题': 'Theme',
    '开机自动启动': 'Start with Windows', '识别后自动复制': 'Auto-copy result',
    '复制后自动贴图': 'Auto-pin after copy', '错误': 'Error', '警告': 'Warning',
    '信息': 'Info', '调试': 'Debug', '日志级别': 'Log level',
    # TitleBar
    '最小化': 'Minimize',
    # Application（状态/错误/气泡提示）
    '已复制颜色 %1': 'Copied color %1', '复制颜色失败：%1': 'Failed to copy color: %1',
    '%1，已临时改用 F1。可在设置中更换截图热键。':
        '%1 Temporarily using F1 instead. You can change the capture hotkey in Settings.',
    '%1，且默认 F1 也不可用。请在设置中更换截图热键。':
        '%1 and the default F1 is also unavailable. Please change the capture hotkey in Settings.',
    '%1，已临时改用 F4。可在设置中更换取字热键。':
        '%1 Temporarily using F4 instead. You can change the OCR hotkey in Settings.',
    '%1，且默认 F4 也不可用。请在设置中更换取字热键。':
        '%1 and the default F4 is also unavailable. Please change the OCR hotkey in Settings.',
    '%1 秒后截图…': 'Capturing in %1 s…', '取字：拖拽选择区域': 'Grab text: drag to select',
    '截图：拖拽选择区域': 'Capture: drag to select', '%1 秒后取字…': 'Grabbing text in %1 s…',
    '长截图完成（%1 × %2），已复制到剪贴板':
        'Long screenshot done (%1 × %2), copied to clipboard',
    '长截图完成（%1 × %2），但复制失败：%3':
        'Long screenshot done (%1 × %2) but copy failed: %3',
    '长截图失败：%1': 'Long screenshot failed: %1', '已取消长截图': 'Long screenshot cancelled',
    '贴图再标注：Enter 复制，Ctrl+S 保存，Esc 关闭':
        'Annotating pinned image: Enter to copy, Ctrl+S to save, Esc to close',
    '还没有历史截图：先截一张再试': 'No screenshots yet. Capture one first.',
    '历史截图读取失败': 'Failed to read the history image',
    '贴图取字：识别中…': 'Grab text from pinned image: recognizing…',
    '已贴图（滚轮缩放，R 旋转，Esc 关闭）':
        'Pinned (wheel to zoom, R to rotate, Esc to close)',
    '已复制到剪贴板（%1 × %2）': 'Copied to clipboard (%1 × %2)',
    '复制失败：%1': 'Copy failed: %1', '已保存：%1': 'Saved: %1',
    '保存失败：%1': 'Save failed: %1', '没有可识别的图像': 'No image to recognize',
    '正在识别…（离线本地识别）': 'Recognizing… (offline)',
    '上一次识别还没结束，请稍候': 'Previous recognition is still running, please wait',
    '识别文本已复制到剪贴板': 'Recognized text copied to clipboard',
    '正在重新识别…': 'Retrying…',
    '已识别 %1 行文字并复制到剪贴板（%2ms）':
        'Recognized %1 lines and copied to clipboard (%2 ms)',
    '已识别 %1 行文字（%2ms）': 'Recognized %1 lines (%2 ms)',
    '设置已保存': 'Settings saved', '设置保存失败：%1': 'Failed to save settings: %1',
    '捕获失败': 'Capture failed',
}


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    root = ET.parse(path).getroot()

    groups = defaultdict(list)
    for context in root.findall('context'):
        for message in context.findall('message'):
            locations = message.findall('location')
            filename = locations[0].get('filename') if locations else ''
            base = filename.replace('\\', '/').rsplit('/', 1)[-1]
            groups[CONTEXT_BY_FILE.get(base, base or 'unknown')].append(
                message.find('source').text)

    missing = []
    lines = ['<?xml version="1.0" encoding="utf-8"?>', '<!DOCTYPE TS>',
             '<TS version="2.1" language="en_US">']
    for context in sorted(groups):
        lines.append('<context>')
        lines.append(f'    <name>{context}</name>')
        seen = set()
        for source in groups[context]:
            if source in seen:
                continue
            seen.add(source)
            english = CONTEXT_OVERRIDES.get((context, source), TR_EN.get(source))
            if english is None:
                missing.append((context, source))
                english = ''
            lines.append('    <message>')
            lines.append(f'        <source>{source}</source>')
            if english:
                lines.append(f'        <translation>{english}</translation>')
            else:
                lines.append('        <translation type="unfinished"></translation>')
            lines.append('    </message>')
        lines.append('</context>')
    lines.append('</TS>')

    with open(path, 'w', encoding='utf-8', newline='') as handle:
        handle.write('\n'.join(lines) + '\n')

    total = sum(len(v) for v in groups.values())
    print(f'contexts={len(groups)} messages={total} missing={len(missing)}')
    for context, source in missing:
        print(f'  MISSING [{context}] {source}')
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main())
