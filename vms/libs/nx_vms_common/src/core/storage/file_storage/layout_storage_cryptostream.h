// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <utils/crypt/crypted_file_stream.h>

#include "layout_storage_stream.h"
#include "layout_storage_resource.h"

class NX_VMS_COMMON_API QnLayoutCryptoStream:
    public nx::utils::CryptedFileStream,
    public QnLayoutStreamSupport
{
public:
    QnLayoutCryptoStream(QnLayoutFileStorageResource& storageResource,
        const QString& fileName, const QString& password);

    virtual ~QnLayoutCryptoStream();

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual void close() override;

    virtual void storeStateAndClose() override;
    virtual void restoreState() override;

private:
    QnLayoutFileStorageResource& m_storageResource;
    QString m_streamName;

    // Used for store/restore functionality that is used for NOV files moving.
    OpenMode m_storedOpenMode = NotOpen;
    qint64 m_storedPosition = 0;

};
