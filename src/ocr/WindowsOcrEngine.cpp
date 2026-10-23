#include "ocr/WindowsOcrEngine.h"

#include "ocr/OcrLogging.h"
#include "ocr/OcrTextUtils.h"

#include <cstdint>
#include <cstring>
#include <exception>
#include <utility>
#include <vector>

#include <QElapsedTimer>
#include <QImage>
#include <QStringList>

// C++/WinRT 头文件必须包含 base.h 与 Foundation/Collections 投影，
// 否则 auto 返回类型（begin()/Size()/get()）会报 C3779 未定义。
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

namespace snapocr::ocr {
namespace {

// 无可用 OCR 语言时的统一中文提示（需求 F-OCR-06：给出安装引导，不崩溃、不拉起 UAC）。
const QString kNoLanguageMessage = QStringLiteral(
    "系统未安装 OCR 语言包，请在“Windows 设置 → 时间和语言 → 语言和区域”中添加语言，"
    "并安装该语言的“光学字符识别”可选功能后重试。");

QString toQString(const winrt::hstring& value)
{
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(value.c_str()),
                              static_cast<qsizetype>(value.size()));
}

QString hresultToHex(const winrt::hresult& code)
{
    return QStringLiteral("0x%1").arg(static_cast<quint32>(code.value), 8, 16, QLatin1Char('0'));
}

// 把 Windows 异常转换成面向用户的中文提示；保留错误码便于反馈问题时定位。
// 注意：这里只记录错误码，不记录任何图像/识别文本内容。
QString hresultToUserMessage(const winrt::hresult_error& error)
{
    const quint32 code = static_cast<quint32>(error.code().value);
    if (code == 0x80070057u)  // E_INVALIDARG
    {
        return QStringLiteral("OCR 识别失败：图像格式或尺寸不受支持，请缩小识别区域后重试。");
    }
    if (code == 0x8007000Eu)  // E_OUTOFMEMORY
    {
        return QStringLiteral("OCR 识别失败：内存不足，请缩小识别区域后重试。");
    }
    if (code == 0x80040154u)  // REGDB_E_CLASSNOTREG
    {
        return QStringLiteral("OCR 识别失败：系统未安装可用的 Windows OCR 组件。");
    }
    return QStringLiteral("OCR 识别失败，请重试（错误码 %1）。").arg(hresultToHex(error.code()));
}

// 初始化当前线程的 COM 公寓。
// recognize() 在 worker 线程使用多线程公寓；语言枚举在主线程优先使用单线程公寓
// （与 Qt/Windows 的 OLE 初始化兼容）。已初始化时 init_apartment 会抛
// RPC_E_CHANGED_MODE，这里忽略并沿用现有公寓。
void ensureApartment(bool multiThreaded)
{
    try
    {
        winrt::init_apartment(multiThreaded ? winrt::apartment_type::multi_threaded
                                            : winrt::apartment_type::single_threaded);
    }
    catch (const winrt::hresult_error& error)
    {
        qCDebug(lcOcr) << "当前线程的 COM 公寓已初始化，沿用现有模式：" << hresultToHex(error.code());
    }
}

// QImage -> SoftwareBitmap（BGRA8，预乘 alpha）。
// QImage::Format_ARGB32_Premultiplied 在小端机器上的内存字节序恰好是 B,G,R,A，
// 与 BitmapPixelFormat::Bgra8 + BitmapAlphaMode::Premultiplied 对应；
// 逐行拷贝用于去掉 QImage 可能的行对齐填充（bytesPerLine 不一定等于 width * 4）。
winrt::Windows::Graphics::Imaging::SoftwareBitmap createSoftwareBitmap(const QImage& image)
{
    const QImage premultiplied = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int width = premultiplied.width();
    const int height = premultiplied.height();
    const qsizetype rowBytes = static_cast<qsizetype>(width) * 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(rowBytes) * static_cast<size_t>(height));

    for (int row = 0; row < height; ++row)
    {
        std::memcpy(pixels.data() + static_cast<size_t>(row) * static_cast<size_t>(rowBytes),
                    premultiplied.constScanLine(row), static_cast<size_t>(rowBytes));
    }

    // 用 DataWriter 把连续内存包装成 IBuffer：DetachBuffer() 返回的缓冲区长度等于已写入字节数，
    // 这是 C++/WinRT 公开支持的路径，不依赖 IBufferByteAccess 等非投影互操作接口。
    winrt::Windows::Storage::Streams::DataWriter writer;
    writer.WriteBytes(
        winrt::array_view<uint8_t const>(pixels.data(), pixels.data() + pixels.size()));
    const winrt::Windows::Storage::Streams::IBuffer buffer = writer.DetachBuffer();
    return winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(
        buffer,
        winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
        width,
        height,
        winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied);
}

// WinRT 的 OcrLine 只提供 Words（每个 OcrWord 带 BoundingRect），
// 这里把整行所有单词的包围盒合并成行包围盒（仍为切片内像素坐标）。
OcrLine toOcrLine(const winrt::Windows::Media::Ocr::OcrLine& winrtLine)
{
    OcrLine line;
    line.text = toQString(winrtLine.Text());

    QRectF boundingBox;
    for (const auto& word : winrtLine.Words())
    {
        const winrt::Windows::Foundation::Rect rect = word.BoundingRect();
        const QRectF wordRect(rect.X, rect.Y, rect.Width, rect.Height);
        boundingBox = boundingBox.isNull() ? wordRect : boundingBox.united(wordRect);
    }
    line.boundingBox = boundingBox;
    return line;
}

// 选择识别语言并创建 OcrEngine；失败时返回空对象并写入中文原因。
winrt::Windows::Media::Ocr::OcrEngine createEngine(const QString& languageTag,
                                                   QString* errorMessage)
{
    using winrt::Windows::Globalization::Language;
    using winrt::Windows::Media::Ocr::OcrEngine;

    const QString trimmedTag = languageTag.trimmed();
    if (!trimmedTag.isEmpty())
    {
        Language language{nullptr};
        try
        {
            language = Language{winrt::hstring(trimmedTag.toStdWString())};
        }
        catch (const winrt::hresult_error&)
        {
            // 非法的 BCP-47 语言标签：给出明确提示，避免被归为“图像格式不受支持”。
            *errorMessage =
                QStringLiteral("OCR 识别语言标签无效「%1」，请使用系统可用语言（如 zh-Hans-CN）。")
                    .arg(trimmedTag);
            return nullptr;
        }
        if (!OcrEngine::IsLanguageSupported(language))
        {
            *errorMessage = QStringLiteral(
                                "系统未安装 OCR 语言包「%1」，请先在 Windows 设置中安装该语言及"
                                "“光学字符识别”功能后重试。")
                                .arg(trimmedTag);
            return nullptr;
        }
        const OcrEngine engine = OcrEngine::TryCreateFromLanguage(language);
        if (!engine)
        {
            *errorMessage = QStringLiteral("OCR 引擎无法使用语言「%1」，请更换识别语言后重试。")
                                .arg(trimmedTag);
        }
        return engine;
    }

    OcrEngine engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (engine)
    {
        return engine;
    }

    // 用户配置语言不支持 OCR（例如系统显示语言没有语言包）时，回退到第一个可用语言。
    const auto languages = OcrEngine::AvailableRecognizerLanguages();
    if (languages.Size() > 0u)
    {
        engine = OcrEngine::TryCreateFromLanguage(languages.GetAt(0));
        if (engine)
        {
            return engine;
        }
    }
    *errorMessage = kNoLanguageMessage;
    return nullptr;
}

}  // namespace

WindowsOcrEngine::WindowsOcrEngine() = default;

WindowsOcrEngine::~WindowsOcrEngine() = default;

QString WindowsOcrEngine::id() const
{
    return QStringLiteral("windows");
}

QVector<OcrLanguage> WindowsOcrEngine::availableLanguages() const
{
    QVector<OcrLanguage> languages;
    try
    {
        ensureApartment(false);
        const auto recognizerLanguages =
            winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages();
        for (const auto& language : recognizerLanguages)
        {
            OcrLanguage item;
            item.tag = toQString(language.LanguageTag());
            item.displayName = toQString(language.DisplayName());
            if (item.displayName.isEmpty())
            {
                item.displayName = item.tag;
            }
            if (!item.tag.isEmpty())
            {
                languages.append(item);
            }
        }
    }
    catch (const winrt::hresult_error& error)
    {
        qCWarning(lcOcr) << "枚举 Windows OCR 语言失败：" << hresultToHex(error.code());
    }
    catch (const std::exception& error)
    {
        qCWarning(lcOcr) << "枚举 Windows OCR 语言异常：" << error.what();
    }
    return languages;
}

bool WindowsOcrEngine::isAvailable(QString* reason) const
{
    const auto setReason = [reason](const QString& text) {
        if (reason != nullptr)
        {
            *reason = text;
        }
    };

    try
    {
        ensureApartment(false);
        QString engineError;
        if (!createEngine(QString(), &engineError))
        {
            setReason(engineError.isEmpty() ? kNoLanguageMessage : engineError);
            return false;
        }
        setReason(QString());
        return true;
    }
    catch (const winrt::hresult_error& error)
    {
        qCWarning(lcOcr) << "检查 Windows OCR 可用性失败：" << hresultToHex(error.code());
        setReason(QStringLiteral("无法访问 Windows OCR 组件，请确认系统版本受支持（错误码 %1）。")
                      .arg(hresultToHex(error.code())));
        return false;
    }
    catch (const std::exception& error)
    {
        qCWarning(lcOcr) << "检查 Windows OCR 可用性异常：" << error.what();
        setReason(QStringLiteral("无法访问 Windows OCR 组件：内部错误，请重试。"));
        return false;
    }
}

OcrResult WindowsOcrEngine::recognize(const QImage& image, const QString& languageTag)
{
    OcrResult result;
    result.languageTag = languageTag;

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    if (image.isNull())
    {
        result.errorMessage = QStringLiteral("待识别图像为空，无法识别。");
        result.elapsedMs = elapsedTimer.elapsed();
        return result;
    }

    try
    {
        ensureApartment(true);

        QString engineError;
        const auto engine = createEngine(languageTag, &engineError);
        if (!engine)
        {
            result.errorMessage = engineError.isEmpty() ? kNoLanguageMessage : engineError;
            result.elapsedMs = elapsedTimer.elapsed();
            return result;
        }
        result.languageTag = toQString(engine.RecognizerLanguage().LanguageTag());

        // 超过 MaxImageDimension 的图像切块识别；重叠区域产生的重复行由 OcrTextUtils 合并。
        const int maxDimension = static_cast<int>(
            winrt::Windows::Media::Ocr::OcrEngine::MaxImageDimension());
        const QVector<QRect> slices = planSlices(image.size(), maxDimension);
        const QRect fullImageRect = image.rect();

        QVector<OcrLine> lines;
        for (const QRect& sliceRect : slices)
        {
            // 整图识别时避免一次多余的深拷贝（QImage 隐式共享不会复制像素）。
            const QImage sliceImage = sliceRect == fullImageRect ? image : image.copy(sliceRect);
            const auto bitmap = createSoftwareBitmap(sliceImage);

            // worker 线程内阻塞等待异步识别；异常统一由外层 catch 转成中文错误。
            const auto ocrResult = engine.RecognizeAsync(bitmap).get();
            for (const auto& winrtLine : ocrResult.Lines())
            {
                OcrLine line = toOcrLine(winrtLine);
                // 分块偏移还原为整图像素坐标。
                line.boundingBox.translate(sliceRect.x(), sliceRect.y());
                if (!line.text.trimmed().isEmpty())
                {
                    lines.append(line);
                }
            }
        }

        result.lines = mergeDuplicateLines(lines);
        QStringList textLines;
        textLines.reserve(result.lines.size());
        for (const OcrLine& line : std::as_const(result.lines))
        {
            textLines.append(line.text);
        }
        result.fullText = textLines.join(QLatin1Char('\n'));
        result.ok = true;
    }
    catch (const winrt::hresult_error& error)
    {
        result.ok = false;
        result.lines.clear();
        result.fullText.clear();
        result.errorMessage = hresultToUserMessage(error);
        qCWarning(lcOcr) << "Windows OCR 识别失败：" << hresultToHex(error.code())
                         << "图像尺寸：" << image.size();
    }
    catch (const std::exception& error)
    {
        result.ok = false;
        result.lines.clear();
        result.fullText.clear();
        result.errorMessage = QStringLiteral("OCR 识别失败：内部异常，请重试。");
        qCWarning(lcOcr) << "Windows OCR 识别异常：" << error.what();
    }
    catch (...)
    {
        result.ok = false;
        result.lines.clear();
        result.fullText.clear();
        result.errorMessage = QStringLiteral("OCR 识别失败：未知错误，请重试。");
        qCWarning(lcOcr) << "Windows OCR 识别出现未知异常。";
    }

    result.elapsedMs = elapsedTimer.elapsed();
    return result;
}

}  // namespace snapocr::ocr
