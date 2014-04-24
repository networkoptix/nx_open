#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    /*void fromApiToResource(const ApiLayoutItemData& data, QnLayoutItemData& resource);
    void fromResourceToApi(QnLayoutResourcePtr resource, ApiLayoutData& data);

    void fromResourceToApi(const QnLayoutItemData& resource, ApiLayoutItemData& data);
    void fromApiToResource(const ApiLayoutData& data, QnLayoutResourcePtr resource);*/


    //QN_DEFINE_STRUCT_SQL_BINDER(ApiLayoutItemData, ApiLayoutItemFields);


    struct ApiLayoutItemData: ApiData {
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

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiLayoutItemData_Fields (uuid)(flags)(left)(top)(right)(bottom)(rotation)(resourceId)(resourcePath) \
                                    (zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetUuid)(contrastParams)(dewarpingParams)

    struct ApiLayoutData : ApiResourceData {
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

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiLayoutData_Fields (cellAspectRatio)(cellSpacingWidth)(cellSpacingHeight)(items)(userCanEdit)(locked) \
                                (backgroundImageFilename)(backgroundWidth)(backgroundHeight)(backgroundOpacity) (userId)


    struct ApiLayoutItemWithRefData: ApiLayoutItemData {
        QnId layoutId;
    };

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiLayoutData, ApiLayoutFields);

    /*struct ApiLayoutList: std::vector<ApiLayoutData>
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
    };*/
}

#endif  //EC2_LAYOUT_DATA_H
