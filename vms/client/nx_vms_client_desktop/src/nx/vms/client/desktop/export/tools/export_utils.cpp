// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_utils.h"

#include <core/resource/storage_resource.h>
#include <export/sign_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

namespace {

// 16Kb ought to be enough for anybody.
static const int kMetadataSeekSizeBytes = 16 * 1024;

bool updateInFile(QIODevice* file,
    QnAviArchiveMetadata::Format fileFormat,
    const QByteArray& source,
    const QByteArray& target)
{
    NX_ASSERT(file);
    NX_ASSERT(source.size() == target.size());

    auto pos = -1;
    switch (fileFormat)
    {
        // Mp4 stores metadata at the end of file.
        case QnAviArchiveMetadata::Format::mp4:
        {
            const auto offset = file->size() - kMetadataSeekSizeBytes;
            file->seek(offset);
            const auto data = file->read(kMetadataSeekSizeBytes);
            pos = data.indexOf(source);
            if (pos >= 0)
                pos += offset;
            break;
        }
        default:
        {
            file->seek(0);
            const auto data = file->read(kMetadataSeekSizeBytes);
            pos = data.indexOf(source);
            break;
        }
    }

    if (pos < 0)
        return false;

    file->seek(pos);
    file->write(target);
    return true;
}

} // namespace


namespace nx::vms::client::desktop {

bool updateSignatureInFile(
    QnStorageResourcePtr storage,
    const QString& fileName,
    QnAviArchiveMetadata::Format fileFormat,
    const QByteArray& signature)
{
    NX_VERBOSE(NX_SCOPE_TAG, "SignVideo: update signature of '%1'",
        nx::utils::url::hidePassword(fileName));

    QScopedPointer<QIODevice> file(storage->open(fileName, QIODevice::ReadWrite));
    if (!file)
    {
        NX_WARNING(NX_SCOPE_TAG, "SignVideo: could not open the file '%1'",
            nx::utils::url::hidePassword(fileName));
        return false;
    }

    QByteArray placeholder = QnSignHelper::addSignatureFiller(QnSignHelper::getSignMagic());
    QByteArray signatureFilled =
        QnSignHelper::addSignatureFiller(signature);

    //New metadata is stored as json, so signature is written base64 - encoded.
    return updateInFile(file.data(),
        fileFormat,
        placeholder.toBase64(),
        signatureFilled.toBase64());
}

} // namespace nx::vms::client::desktop
