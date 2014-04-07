#include "ec2_videowall_data.h"
#include "core/resource/videowall_resource.h"

namespace ec2
{

    void ApiVideowallData::toResource(QnVideoWallResourcePtr resource) const
    {
        ApiResourceData::toResource(resource);
        resource->setAutorun(autorun);
    }
    
    void ApiVideowallData::fromResource(QnVideoWallResourcePtr resource)
    {
        ApiResourceData::fromResource(resource);
        autorun = resource->isAutorun();
    }

    template <class T>
    void ApiVideowallDataList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnVideoWallResourcePtr videowall(new QnVideoWallResource());
            data[i].toResource(videowall);
            outData << videowall;
        }
    }
    template void ApiVideowallDataList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiVideowallDataList::toResourceList<QnVideoWallResourcePtr>(QList<QnVideoWallResourcePtr>& outData) const;

    void ApiVideowallDataList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiVideowallData, data, ApiVideowallDataFields ApiResourceDataFields)
    }

}
