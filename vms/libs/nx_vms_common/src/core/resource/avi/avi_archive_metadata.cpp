// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "avi_archive_metadata.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <export/sign_helper.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>
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

// TODO: #rvasilenko We have two independent and non-standard ways to (de)serialize such layout.
// Looks like we need to unify the approach.
bool deserializeLayout(const QString& layout, QnAviArchiveMetadata& metadata)
{
    QStringList info = layout.split(';');
    for (int i = 0; i < info.size(); ++i)
    {
        QStringList params = info[i].split(',');
        if (params.size() != 2)
        {
            NX_WARNING(typeid(QnAviArchiveMetadata), lit("Invalid layout string stored at file metadata: \"%1\"").arg(layout));
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
                NX_WARNING(typeid(QnAviArchiveMetadata),
                    lit("Invalid layout string stored at file metadata: %1"), layout);
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

static int getVideoRotation(const AVFormatContext* context)
{
    std::optional<int> result;
    for (int i = 0; i < context->nb_streams; ++i)
    {
        if (context->streams[i]->codecpar
            && context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (result)
                return 0; //< Won't support more than one video stream.

            auto entry = av_dict_get(context->streams[i]->metadata, "rotate", NULL, 0);
            if (entry && entry->value)
                result = QString(entry->value).toInt();
            else
                result = 0;
        }
    }

    return result ? *result : 0;
}

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAviArchiveMetadata, (json), QnAviArchiveMetadata_Fields)

QnAviArchiveMetadata QnAviArchiveMetadata::loadFromFile(const AVFormatContext* context)
{
    // Prevent standard tag name parsing in 'avi' format.
    const QString format = QString::fromLatin1(context->iformat->name).split(QLatin1Char(','))[0];

    const auto metadata = tagValueRaw(context, CustomTag, format);
    QnAviArchiveMetadata result = QJson::deserialized<QnAviArchiveMetadata>(metadata);
    auto [validate, success] = nx::reflect::json::deserialize<QnAviArchiveMetadata>(
        metadata.toStdString());
    // FIXME: #sivanov Switch to nx_reflect in master branch.
    NX_ASSERT(result == validate, "Fusion vs reflect serialization mismatch");

    if (result.version >= kVersionBeforeTheIntegrityCheck)
        return result;

    // All following code is actual only to support files exported by VMS 2.6 and older.

    result.signature = tagValueRaw(context, SignatureTag, format);

    // Check time zone in the sign's 4-th column.
    QList<QByteArray> tmp = result.signature.split(QnSignHelper::getSignPatternDelim());
    if (tmp.size() > 4)
    {
        bool deserialized = false;
        qint64 value = tmp[4].trimmed().toLongLong(&deserialized);
        if (deserialized && value != -1)
            result.timeZoneOffset = std::chrono::milliseconds(value);
    }

    result.rotation = getVideoRotation(context);
    QString software = tagValue(context, SoftwareTag, format);
    bool allowTags = format != "avi"
        || (!software.isEmpty() && software.front() == 'N' && software.back() == 'x');
    if (!allowTags)
        return result;

    // Set marker of invalid layout in case of error.
    if (!deserializeLayout(tagValue(context, LayoutInfoTag, format), result))
        result.videoLayoutSize = QSize();

    result.startTimeMs = tagValue(context, StartTimeTag, format).toLongLong();
    result.dewarpingParams = nx::vms::api::dewarping::MediaData::fromByteArray(
        tagValueRaw(context, DewarpingTag, format));

    return result;
}

bool QnAviArchiveMetadata::saveToFile(AVFormatContext* context, Format format)
{
    bool isSuccessful = true;

    auto setValueLogged =
        [this, &isSuccessful, context, format](Tag tag, const QByteArray& value)
        {
            static const int kFlags = 0; //< No additional flags are needed.
            const int errCode = av_dict_set(&context->metadata,
                getTagName(tag, format),
                value,
                kFlags);

            if (errCode >= 0)
                return;

            isSuccessful = false;
            NX_WARNING(this, "Error writing metadata %1: %2", getTagName(tag, format), errCode);
        };

    // FIXME: #sivanov Switch to nx_reflect in master branch.
    setValueLogged(CustomTag, QJson::serialized<QnAviArchiveMetadata>(*this));
    QByteArray validate = QByteArray::fromStdString(nx::reflect::json::serialize(*this));
    NX_ASSERT(QJson::deserialized<QnAviArchiveMetadata>(validate) == *this,
        "Fusion vs reflect serialization mismatch");

    // Other tags are not supported in mp4 format.
    if (format == Format::mp4)
        return isSuccessful;

    if (!videoLayoutSize.isEmpty())
    {
        // Keep old format for compatibility with old clients.
        const auto layoutStr = serializeLayout(*this);
        setValueLogged(LayoutInfoTag, layoutStr.toLatin1());
    }

    if (startTimeMs > 0)
        setValueLogged(StartTimeTag, QString::number(startTimeMs).toLatin1());

    setValueLogged(SoftwareTag, "Nx");

    // Dewarping exists in resource and is not activated now. Allow dewarping for saved file.
    if (dewarpingParams.enabled)
    {
        setValueLogged(DewarpingTag,
            QJson::serialized<nx::vms::api::dewarping::MediaData>(dewarpingParams));
    }

    return isSuccessful;
}

QnAviArchiveMetadata::Format QnAviArchiveMetadata::formatFromExtension(const QString& extension)
{
    if (extension == "avi")
        return QnAviArchiveMetadata::Format::avi;

    if (extension == "mp4")
        return QnAviArchiveMetadata::Format::mp4;

    return QnAviArchiveMetadata::Format::custom;
}
