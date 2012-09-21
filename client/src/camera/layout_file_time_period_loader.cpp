#include "layout_file_time_period_loader.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"

QnLayoutFileTimePeriodLoader::QnLayoutFileTimePeriodLoader(QnResourcePtr resource, QObject *parent, const QnTimePeriodList& chunks):
    QnAbstractTimePeriodLoader(resource, parent),
    m_chunks(chunks),
    m_handle(0)
{
}

QnLayoutFileTimePeriodLoader* QnLayoutFileTimePeriodLoader::newInstance(QnResourcePtr resource, QObject *parent)
{
    QnAviResourcePtr localFile = resource.dynamicCast<QnAviResource>();
    if (!localFile)
        return 0;
    QnLayoutFileStorageResourcePtr storage = localFile->getStorage().dynamicCast<QnLayoutFileStorageResource>();
    if (!storage)
        return 0;
    QFileInfo fi(resource->getName());
    QIODevice* chunkData = storage->open(QString(QLatin1String("chunk_%1.pb")).arg(fi.baseName()), QIODevice::ReadOnly);
    if (!chunkData)
        return 0;
    QnTimePeriodList chunks;
    QByteArray chunkDataArray(chunkData->readAll());
    chunks.decode(chunkDataArray);
    delete chunkData;
    // todo: add motion info here
    return new QnLayoutFileTimePeriodLoader(resource, parent, chunks);
}

int QnLayoutFileTimePeriodLoader::load(const QnTimePeriod &period, const QList<QRegion> &motionRegions)
{
    bool isNoMotion = true;
    for (int i = 0; i < motionRegions.size(); ++i)
    {
        if (!motionRegions[i].isEmpty()) {
            isNoMotion = false;
            break;
        }
    }

    if (isNoMotion) {
        ++m_handle;
        emit delayedReady(m_chunks, m_handle);
        return m_handle;
    }
    else {
        // todo: implement motion for exported layout here
        return -1;
    }
}
