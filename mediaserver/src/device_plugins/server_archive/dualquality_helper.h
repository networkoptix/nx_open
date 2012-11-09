#ifndef __DUAL_QUALITY_HELPER__
#define __DUAL_QUALITY_HELPER__

#include "recorder/device_file_catalog.h"
#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"

class QnDialQualityHelper
{
public:
    QnDialQualityHelper();

    void setResource(QnNetworkResourcePtr netResource);
    void setPrefferedQuality(MediaQuality quality);
    void findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& resultChunk, DeviceFileCatalogPtr& resultCatalog, DeviceFileCatalog::FindMethod findMethod) const;

    //void findNextChunk(const DeviceFileCatalogPtr& currentCatalog, const DeviceFileCatalog::Chunk& currentChunk, DeviceFileCatalog::Chunk& nextChunk, DeviceFileCatalogPtr& nextCatalog);
private:
    DeviceFileCatalogPtr m_catalogHi;
    DeviceFileCatalogPtr m_catalogLow;
    MediaQuality m_quality;
};

#endif // __DUAL_QUALITY_HELPER__
