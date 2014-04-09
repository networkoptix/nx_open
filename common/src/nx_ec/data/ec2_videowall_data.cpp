#include "ec2_videowall_data.h"
#include "core/resource/videowall_resource.h"

namespace ec2
{

    void ApiVideowall::toResource(QnVideoWallResourcePtr resource) const
    {
        ApiResource::toResource(resource);
        resource->setAutorun(autorun);
    }
    
    void ApiVideowall::fromResource(QnVideoWallResourcePtr resource)
    {
        ApiResource::fromResource(resource);
        autorun = resource->isAutorun();
    }

    template <class T>
    void ApiVideowallList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnVideoWallResourcePtr videowall(new QnVideoWallResource());
            data[i].toResource(videowall);
            outData << videowall;
        }
    }
    template void ApiVideowallList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiVideowallList::toResourceList<QnVideoWallResourcePtr>(QList<QnVideoWallResourcePtr>& outData) const;

    void ApiVideowallList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiVideowall, data, ApiVideowallDataFields ApiResourceFields)
    }

}
