// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include <QtCore/QCoreApplication> // For Q_DECLARE_TR_FUNCTIONS.

#include <optional>

namespace nx::recording {

struct NX_VMS_COMMON_API Error
{
    enum class Code
    {
        unknown,
        containerNotFound,
        fileCreate,
        videoStreamAllocation,
        audioStreamAllocation,
        metadataStreamAllocation,
        invalidAudioCodec,
        incompatibleCodec,
        fileWrite,
        invalidResourceType,
        dataNotFound,
        resourceNotFound,
        transcodingRequired,
        encryptedArchive,
        temporaryUnavailable,
    };

    Code code = Code::unknown;
    QnStorageResourcePtr storage;

    Error(Code code, const QnStorageResourcePtr& storage = QnStorageResourcePtr());
    QString toString() const;

private:
    Q_DECLARE_TR_FUNCTIONS(nx::recording::Error)
};

/**
 * Auxiliary data structure. Intended to be used when additional text error information is desired
 * to be passed around (to be further logged for example).
 */
struct ErrorEx: public Error
{
    QString message;

    ErrorEx(
        Error::Code code,
        const QString& message,
        const QnStorageResourcePtr& storage = QnStorageResourcePtr())
        :
        Error(code, storage),
        message(message)
    {}
};

} // namespace nx::recording
