// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/data/dewarping_data.h>

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

    int version = kDefaultVersion;
    QByteArray signature;
    qint64 timeZoneOffset = Qn::InvalidUtcOffset;
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
};

#define QnAviArchiveMetadata_Fields (version)(signature)(timeZoneOffset)(startTimeMs) \
    (videoLayoutChannels)(videoLayoutSize)(dewarpingParams)(overridenAr)(integrityHash)\
    (metadataStreamVersion)(encryptionData)(rotation)

QN_FUSION_DECLARE_FUNCTIONS(QnAviArchiveMetadata, (json), NX_VMS_COMMON_API)
