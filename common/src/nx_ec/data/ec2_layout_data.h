#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "ec2_resource_data.h"
#include "core/resource/layout_item_data.h"

namespace ec2
{
    struct ApiLayoutItemData: public ApiData
    {
        QByteArray uuid;
        qint32 flags;
        float left;
        float top;
        float right;
        float bottom;
        float rotation;
        QnId resourceId;
        QString resourcePath;
        float zoomLeft;
        float zoomTop;
        float zoomRight;
        float zoomBottom;
        QByteArray zoomTargetUuid;
        QByteArray contrastParams;
        QByteArray dewarpingParams;

        void toResource(QnLayoutItemData& resource) const;
        void fromResource(const QnLayoutItemData& resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiLayoutItemDataFields (uuid) (flags) (left) (top) (right) (bottom) (rotation) (resourceId) (resourcePath) (zoomLeft) (zoomTop) (zoomRight) (zoomBottom) (zoomTargetUuid) (contrastParams) (dewarpingParams)
    QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ApiLayoutItemData, ApiLayoutItemDataFields)
}



namespace ec2
{
    struct ApiLayoutItemDataWithRef: public ApiLayoutItemData {
        QnId layoutId;
    };

    struct ApiLayoutData: public ApiResource
    {
        float cellAspectRatio;
        float cellSpacingWidth;
        float cellSpacingHeight;
        std::vector<ApiLayoutItemData> items;
        bool   userCanEdit;
        bool   locked;
        QString backgroundImageFilename;
        qint32  backgroundWidth;
        qint32  backgroundHeight;
        float backgroundOpacity;
        qint32 userId;

        void toResource(QnLayoutResourcePtr resource) const;
        void fromResource(QnLayoutResourcePtr resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiLayoutDataFields (cellAspectRatio) (cellSpacingWidth) (cellSpacingHeight) (items) (userCanEdit) (locked) (backgroundImageFilename) (backgroundWidth) (backgroundHeight) (backgroundOpacity) (userId)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ApiLayoutData, ec2::ApiResource, ApiLayoutDataFields)
}
                                                                                

namespace ec2
{
    struct ApiLayoutDataList: public ApiData
    {
        std::vector<ApiLayoutData> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
        template <class T> void fromResourceList(const QList<T>& srcData)
        {
            data.reserve( srcData.size() );
            for( const T& layoutRes: srcData )
            {
                data.push_back( ApiLayoutData() );
                data.back().fromResource( layoutRes );
            }
        }
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiLayoutDataList, (data) )
}

#endif  //EC2_LAYOUT_DATA_H
