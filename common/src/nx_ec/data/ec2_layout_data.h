#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "ec2_resource_data.h"
#include "core/resource/layout_item_data.h"

namespace ec2
{
    struct ApiLayoutItemData: public ApiData
    {
        qint32 id;
        QByteArray uuid;
        qint32 flags;
        float left;
        float top;
        float right;
        float bottom;
        float rotation;
        qint32 layoutId;
        qint32 resourceId;
        float zoomLeft;
        float zoomTop;
        float zoomRight;
        float zoomBottom;
        QByteArray zoomTargetUuid;
        QByteArray contrastParams;
        QByteArray dewarpingParams;

        void toResource(QnLayoutItemData& resource) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    struct ApiLayoutData: public ApiResourceData
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
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    struct ApiLayoutDataList: public ApiData
    {
        std::vector<ApiLayoutData> data;

        void loadFromQuery(QSqlQuery& query);
        void toLayoutList(QnLayoutResourceList& outData) const;
    };
}

#define ApiLayoutItemDataFields (id) (uuid) (flags) (left) (top) (right) (bottom) (rotation) (layoutId) (resourceId) (zoomLeft) (zoomTop) (zoomRight) (zoomBottom) (zoomTargetUuid) (contrastParams) (dewarpingParams)
#define ApiLayoutDataFields (cellAspectRatio) (cellSpacingWidth) (cellSpacingHeight) (items) (userCanEdit) (locked) (backgroundImageFilename) (backgroundWidth) (backgroundHeight) (backgroundOpacity) (userId)

QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiLayoutItemData, ApiLayoutItemDataFields)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiLayoutData, ec2::ApiResourceData, ApiLayoutDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiLayoutDataList, (data) )

#endif  //EC2_LAYOUT_DATA_H
