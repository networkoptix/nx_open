#ifndef __DUAL_QUALITY_HELPER__
#define __DUAL_QUALITY_HELPER__

#include <QtCore/QUuid>

#include "recorder/device_file_catalog.h"
#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"

class QnDualQualityHelper
{
public:
    QnDualQualityHelper();

    void setResource(const QnNetworkResourcePtr &netResource);
    void openCamera(const QString& cameraUniqueId);
    void setPrefferedQuality(MediaQuality quality);
    void findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& resultChunk, DeviceFileCatalogPtr& resultCatalog, DeviceFileCatalog::FindMethod findMethod, bool preciseFind);
    //void findNextChunk(const DeviceFileCatalogPtr& currentCatalog, const DeviceFileCatalog::Chunk& currentChunk, DeviceFileCatalog::Chunk& nextChunk, DeviceFileCatalogPtr& nextCatalog);
private:
    DeviceFileCatalogPtr m_catalogHi;
    DeviceFileCatalogPtr m_catalogLow;
    MediaQuality m_quality;
    bool m_alreadyOnAltChunk;
};

#endif // __DUAL_QUALITY_HELPER__
