#ifndef __DUAL_QUALITY_HELPER__
#define __DUAL_QUALITY_HELPER__

#include "recorder/device_file_catalog.h"
#include "core/resource/resource_fwd.h"
#include "core/datapacket/mediadatapacket.h"

class QnDialQualityHelper
{
public:
    QnDialQualityHelper();

    void setResource(QnNetworkResourcePtr netResource);
    void setPrefferedQuality(MediaQuality quality);
    void setFindMethod(DeviceFileCatalog::FindMethod findMethod);

    void findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& resultChunk, DeviceFileCatalogPtr& resultCatalog);

    //void findNextChunk(const DeviceFileCatalogPtr& currentCatalog, const DeviceFileCatalog::Chunk& currentChunk, DeviceFileCatalog::Chunk& nextChunk, DeviceFileCatalogPtr& nextCatalog);
private:
    DeviceFileCatalogPtr m_catalogHi;
    DeviceFileCatalogPtr m_catalogLow;
    MediaQuality m_quality;
    DeviceFileCatalog::FindMethod m_findMethod;
};

#endif // __DUAL_QUALITY_HELPER__
