// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "single_shot_file_reader.h"

#include <common/common_module.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/storage_resource.h>
#include <nx/streaming/config.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/vms/common/application_context.h>
#include <utils/common/synctime.h>

#include "filetypesupport.h"

using namespace nx::vms::common;

QnSingleShotFileStreamreader::QnSingleShotFileStreamreader(
    const QnResourcePtr& resource)
    :
    QnAbstractMediaStreamDataProvider(resource)
{
}

QnAbstractMediaDataPtr QnSingleShotFileStreamreader::getNextData()
{
    AVCodecID compressionType;
    QString lowerFileName = getResource()->getUrl().toLower();


    if (lowerFileName.endsWith(QLatin1String(".png")))
        compressionType = AV_CODEC_ID_PNG;
    else if (lowerFileName.endsWith(QLatin1String(".jpeg")) || lowerFileName.endsWith(QLatin1String(".jpg")))
        compressionType = AV_CODEC_ID_MJPEG;
    else if (lowerFileName.endsWith(QLatin1String(".tiff")) || lowerFileName.endsWith(QLatin1String(".tif")))
        compressionType = AV_CODEC_ID_TIFF;
    else if (lowerFileName.endsWith(QLatin1String(".gif")))
        compressionType = AV_CODEC_ID_GIF;
    else if (lowerFileName.endsWith(QLatin1String(".bmp")))
        compressionType = AV_CODEC_ID_BMP;
    else
        return QnAbstractMediaDataPtr();

    if (!m_storage)
    {
        m_storage = QnStorageResourcePtr(
            appContext()->storagePluginFactory()->createStorage(getResource()->getUrl()));
    }
    QIODevice* file = m_storage->open(getResource()->getUrl(), QIODevice::ReadOnly);
    if (file == 0)
        return QnAbstractMediaDataPtr();

    QByteArray srcData = file->readAll();
    QnWritableCompressedVideoDataPtr outData(new QnWritableCompressedVideoData(srcData.size()));
    outData->m_data.write(srcData);

    outData->compressionType = compressionType;
    outData->flags |= QnAbstractMediaData::MediaFlags_AVKey | QnAbstractMediaData::MediaFlags_StillImage;
    outData->timestamp = qnSyncTime->currentUSecsSinceEpoch();
    outData->dataProvider = this;
    outData->channelNumber = 0;

    delete file;
    return outData;
}

void QnSingleShotFileStreamreader::run()
{
    initSystemThreadId();
    QnAbstractMediaDataPtr data;
    try {
        data = getNextData();
    } catch(...) {
        qWarning() << "Application out of memory";
    }
    if (data)
        putData(std::move(data));
}

void QnSingleShotFileStreamreader::setStorage(const QnStorageResourcePtr& storage)
{
    m_storage = storage;
}

CameraDiagnostics::Result QnSingleShotFileStreamreader::diagnoseMediaStreamConnection()
{
    //TODO/IMPL
    return CameraDiagnostics::NotImplementedResult();
}
