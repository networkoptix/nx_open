#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QSize>

#include <common/common_globals.h>

#include <core/ptz/media_dewarping_params.h>

#include <nx/fusion/model_functions_fwd.h>

struct AVFormatContext;

/**
 * Custom fields in an exported video file.
 */
struct QnAviArchiveMetadata
{
    enum class Format
    {
        avi,
        mp4,
        custom //< Allows to setup any fields
    };

    /** This version is set if no metadata is found. */
    static const int kDefaultVersion = 0;

    static const int kLatestVersion = 1;
    static const int kIntegrityCheckVersion = 2;

    int version = kDefaultVersion;
    QByteArray signature;
    qint64 timeZoneOffset = Qn::InvalidUtcOffset;
    qint64 startTimeMs = 0; //< Start time in milliseconds since epoch.
    QVector<int> videoLayoutChannels;
    QSize videoLayoutSize;
    QnMediaDewarpingParams dewarpingParams;
    qreal overridenAr = 0.0;
    QByteArray integrityHash;

    static QnAviArchiveMetadata loadFromFile(const AVFormatContext* context);
    bool saveToFile(AVFormatContext* context, Format format);
};
#define QnAviArchiveMetadata_Fields (version)(signature)(timeZoneOffset)(startTimeMs) \
    (videoLayoutChannels)(videoLayoutSize)(dewarpingParams)(overridenAr)(integrityHash)

QN_FUSION_DECLARE_FUNCTIONS(QnAviArchiveMetadata, (json)(metatype))
