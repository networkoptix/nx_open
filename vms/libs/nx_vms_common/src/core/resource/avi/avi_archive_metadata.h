// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_geometry_reflect_json.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/time/timezone.h>

struct AVFormatContext;

/**
 * Custom fields in an exported video file.
 */
struct NX_VMS_COMMON_API QnAviArchiveMetadata
{
    NX_REFLECTION_ENUM_CLASS(Format,
        avi,
        mp4,
        custom //< Allows to setup any fields
    );

    /** This version is set if no metadata is found. */
    static const int kDefaultVersion = 0;

    static const int kVersionBeforeTheIntegrityCheck = 1;
    static const int kIntegrityCheckVersion = 2;
    static const int kMetadataStreamVersion_1 = 1;
    static const int kMetadataStreamVersion_2 = 2;
    static const int kMetadataStreamVersion_3 = 3;

    int version = kDefaultVersion;
    QByteArray signature;

    // Time zone offset from UTC in milliseconds.
    std::chrono::milliseconds timeZoneOffset = nx::vms::time::kInvalidUtcOffset;

    QString timeZoneId;

    qint64 startTimeMs = 0; //< Start time in milliseconds since epoch.
    QVector<int> videoLayoutChannels;
    QSize videoLayoutSize;
    nx::vms::api::dewarping::MediaData dewarpingParams;
    qreal overridenAr = 0.0;
    QByteArray integrityHash;
    int metadataStreamVersion = 0;
    std::vector<uint8_t> encryptionData;
    int rotation = 0;

    static QnAviArchiveMetadata loadFromFile(const AVFormatContext* context);
    static Format formatFromExtension(const QString& extension);
    bool saveToFile(AVFormatContext* context, Format format);

    bool operator==(const QnAviArchiveMetadata& other) const = default;
};

#define QnAviArchiveMetadata_Fields (version)(signature)(timeZoneOffset)(timeZoneId)(startTimeMs) \
    (videoLayoutChannels)(videoLayoutSize)(dewarpingParams)(overridenAr)(integrityHash)\
    (metadataStreamVersion)(encryptionData)(rotation)

QN_FUSION_DECLARE_FUNCTIONS(QnAviArchiveMetadata, (json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnAviArchiveMetadata, QnAviArchiveMetadata_Fields)
