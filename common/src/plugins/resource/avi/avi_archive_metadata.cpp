#include "avi_archive_metadata.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <export/sign_helper.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/log.h>

namespace {

enum Tag
{
    StartTimeTag,
    LayoutInfoTag,
    SoftwareTag,
    SignatureTag,
    DewarpingTag,
    CustomTag /**< Tag for all other future values encoded in JSON object. */
};

/**
 * Some containers supports only predefined tag names.
 */
const char* getTagName(Tag tag, QnAviArchiveMetadata::Format format)
{
    switch (format)
    {
        // http://ru.wikipedia.org/wiki/RIFF
        case QnAviArchiveMetadata::Format::avi:
            switch (tag)
            {
                case StartTimeTag:  return "date"; //< "ICRD"
                case LayoutInfoTag: return "comment"; //< "ICMT"
                case SoftwareTag:   return "encoded_by"; //< "ITCH"
                case SignatureTag:  return "copyright"; //< "ICOP"
                case DewarpingTag:  return "title";
                case CustomTag:     return "IENG"; //< IENG
            }
            break;

        // https://wiki.multimedia.cx/index.php/FFmpeg_Metadata
        case QnAviArchiveMetadata::Format::mp4:
            switch (tag)
            {
                case StartTimeTag:  return "episode_id";
                case LayoutInfoTag: return "show";
                case SoftwareTag:   return "synopsis";
                case SignatureTag:  return "copyright";
                case DewarpingTag:  return "description";
                case CustomTag:     return "comment";
            }
            break;

        case QnAviArchiveMetadata::Format::custom:
            switch (tag)
            {
                case StartTimeTag:  return "start_time";
                case LayoutInfoTag: return "video_layout";
                case SoftwareTag:   return "software";
                case SignatureTag:  return "signature";
                case DewarpingTag:  return "dewarp";
                case CustomTag:     return "custom_data";
            }
            break;

        default:
            break;
    }

    return "";
}

/**
 * Some containers supports only predefined tag names.
 */
const char* getTagName(Tag tag, const QString& formatName)
{
    if (formatName == QLatin1String("avi"))
        return getTagName(tag, QnAviArchiveMetadata::Format::avi);

    if (formatName == QLatin1String("mov"))
        return getTagName(tag, QnAviArchiveMetadata::Format::mp4);

    return getTagName(tag, QnAviArchiveMetadata::Format::custom);
}

QByteArray tagValueRaw(const AVFormatContext* context, Tag tag, const QString& format)
{
    AVDictionaryEntry* entry = av_dict_get(context->metadata, getTagName(tag, format), 0, 0);
    if (entry && entry->value)
        return entry->value;

    return QByteArray();
}

QString tagValue(const AVFormatContext* context, Tag tag, const QString& format)
{
    return QString::fromUtf8(tagValueRaw(context, tag, format));
}

//TODO: #rvasilenko we have two independent and non-standard ways to (de)serialize such layout. Looks like hell.
bool deserializeLayout(const QString& layout, QnAviArchiveMetadata& metadata)
{
    QStringList info = layout.split(L';');
    for (int i = 0; i < info.size(); ++i)
    {
        QStringList params = info[i].split(L',');
        if (params.size() != 2)
        {
            NX_LOG(lit("Invalid layout string stored at file metadata: %1").arg(layout), cl_logWARNING);
            return false;
        }
        if (i == 0)
        {
            int w = params[0].toInt();
            int h = params[1].toInt();
            metadata.videoLayoutSize = QSize(w, h);
            metadata.videoLayoutChannels.resize(w * h);
        }
        else
        {
            int x = params[0].toInt();
            int y = params[1].toInt();
            int channel = i - 1;
            int index = y * metadata.videoLayoutSize.width() + x;
            if (index < 0 || index >= metadata.videoLayoutChannels.size())
            {
                NX_LOG(lit("Invalid layout string stored at file metadata: %1").arg(layout), cl_logWARNING);
                return false;
            }
            metadata.videoLayoutChannels[index] = channel;
        }
    }
    return true;
}

// Keeping this to handle compatibility with versions < 3.0
QString serializeLayout(const QnAviArchiveMetadata& metadata)
{
    auto position =
        [&metadata](int index)
        {
            const auto& channels = metadata.videoLayoutChannels;
            auto width = metadata.videoLayoutSize.width();

            for (int i = 0; i < channels.size(); ++i)
            {
                if (channels[i] == index)
                    return QPoint(i % width, i / width);
            }

            return QPoint();
        };

    QString rez;
    QTextStream ost(&rez);
    ost << metadata.videoLayoutSize.width() << ',' << metadata.videoLayoutSize.height();
    for (int index = 0; index < metadata.videoLayoutChannels.size(); ++index)
    {
        auto pos = position(index);
        ost << ';' << pos.x() << ',' << pos.y();
    }
    ost.flush();
    return rez;
}

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAviArchiveMetadata, (json), QnAviArchiveMetadata_Fields)

QnAviArchiveMetadata QnAviArchiveMetadata::loadFromFile(const AVFormatContext* context)
{
    // Prevent standart tag name parsing in 'avi' format.
    const QString format = QString::fromLatin1(context->iformat->name).split(QLatin1Char(','))[0];

    const auto metadata = tagValueRaw(context, CustomTag, format);
    QnAviArchiveMetadata result = QJson::deserialized<QnAviArchiveMetadata>(metadata);
    if (result.version == kLatestVersion)
        return result;

    // For now there are only one version, so no different migration methods are required.

    result.signature = tagValueRaw(context, SignatureTag, format);

    // Check time zone in the sign's 4-th column.
    QList<QByteArray> tmp = result.signature.split(QnSignHelper::getSignPatternDelim());
    if (tmp.size() > 4)
    {
        bool deserialized = false;
        qint64 value = tmp[4].trimmed().toLongLong(&deserialized);
        if (deserialized && value != Qn::InvalidUtcOffset && value != -1)
            result.timeZoneOffset = value;
    }

    QString software = tagValue(context, SoftwareTag, format);
    bool allowTags = format != QLatin1String("avi") || software == QLatin1String("Network Optix");
    if (!allowTags)
        return result;

    // Set marker of invalid layout in case of error.
    if (!deserializeLayout(tagValue(context, LayoutInfoTag, format), result))
        result.videoLayoutSize = QSize();

    result.startTimeMs = tagValue(context, StartTimeTag, format).toLongLong();
    result.dewarpingParams = QnMediaDewarpingParams::deserialized(tagValueRaw(context, DewarpingTag, format));

    return result;
}

void QnAviArchiveMetadata::saveToFile(AVFormatContext* context, Format format)
{
    av_dict_set(
        &context->metadata,
        getTagName(CustomTag, format),
        QJson::serialized<QnAviArchiveMetadata>(*this),
        0
    );

    // Other tags are not supported in mp4 format.
    if (format == QnAviArchiveMetadata::Format::mp4)
        return;

    // Keep old format for compatibility with old clients.

    if (!videoLayoutSize.isEmpty())
    {
        const QString layoutStr = serializeLayout(*this);
        av_dict_set(
            &context->metadata,
            getTagName(LayoutInfoTag, format),
            layoutStr.toLatin1().data(),
            0);
    }

    if (startTimeMs > 0)
    {
        av_dict_set(
            &context->metadata,
            getTagName(StartTimeTag, format),
            QString::number(startTimeMs).toLatin1().data(),
            0
        );
    }

    av_dict_set(
        &context->metadata,
        getTagName(SoftwareTag, format),
        "Network Optix",
        0
    );

    if (dewarpingParams.enabled)
    {
        // Dewarping exists in resource and not activated now. Allow dewarping for saved file.
        av_dict_set(
            &context->metadata,
            getTagName(DewarpingTag, format),
            QJson::serialized<QnMediaDewarpingParams>(dewarpingParams),
            0
        );
    }

    if (!signature.isEmpty())
    {
        av_dict_set(
            &context->metadata,
            getTagName(SignatureTag, format),
            signature.data(),
            0
        );
    }
}
