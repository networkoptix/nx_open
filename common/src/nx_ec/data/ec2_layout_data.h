#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "ec2_resource_data.h"
#include "core/resource/layout_item_data.h"

namespace ec2
{
    struct ApiLayoutItem;
    typedef std::vector<ApiLayoutItem> ApiLayoutItemVector;
    #include "ec2_layout_data_i.h"

    struct ApiLayoutItem: ApiLayoutItemData
    {
        void toResource(QnLayoutItemData& resource) const;
        void fromResource(const QnLayoutItemData& resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiLayoutItem, ApiLayoutItemFields);
}



namespace ec2
{
    struct ApiLayoutItemWithRef: ApiLayoutItem {
        QnId layoutId;
    };

    struct ApiLayout: ApiLayoutData, ApiResource
    {
        void toResource(QnLayoutResourcePtr resource) const;
        void fromResource(QnLayoutResourcePtr resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiLayout, ApiLayoutFields);
}
                                                                                

namespace ec2
{
    struct ApiLayoutList: public ApiData
    {
        std::vector<ApiLayout> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
        template <class T> void fromResourceList(const QList<T>& srcData)
        {
            data.reserve( srcData.size() );
            for( const T& layoutRes: srcData )
            {
                data.push_back( ApiLayout() );
                data.back().fromResource( layoutRes );
            }
        }
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiLayoutList, (data) )
}

#endif  //EC2_LAYOUT_DATA_H
