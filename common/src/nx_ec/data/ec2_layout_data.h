#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "ec2_resource_data.h"
#include "core/resource/layout_item_data.h"

namespace ec2
{
    #include "ec2_layout_data_i.h"

    void fromApiToResource(const ApiLayoutItemData& data, QnLayoutItemData& resource);
    void fromResourceToApi(QnLayoutResourcePtr resource, ApiLayoutData& data);

    void fromResourceToApi(const QnLayoutItemData& resource, ApiLayoutItemData& data);
    void fromApiToResource(const ApiLayoutData& data, QnLayoutResourcePtr resource);


    QN_DEFINE_STRUCT_SQL_BINDER(ApiLayoutItemData, ApiLayoutItemFields);

    struct ApiLayoutItemWithRef: ApiLayoutItemData {
        QnId layoutId;
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiLayoutData, ApiLayoutFields);

    struct ApiLayoutList: ApiLayoutDataListData
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
        template <class T> void fromResourceList(const QList<T>& srcData)
        {
            data.reserve( srcData.size() );
            for( const T& layoutRes: srcData )
            {
                data.push_back( ApiLayoutData() );
                fromResourceToApi(layoutRes, data.back());
            }
        }
    };
}

#endif  //EC2_LAYOUT_DATA_H
