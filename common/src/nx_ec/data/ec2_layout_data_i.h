    struct ApiLayoutItemData: virtual ApiData {
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
    };

    #define ApiLayoutItemFields (uuid) (flags) (left) (top) (right) (bottom) (rotation) (resourceId) (resourcePath) (zoomLeft) (zoomTop) (zoomRight) (zoomBottom) (zoomTargetUuid) (contrastParams) (dewarpingParams)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiLayoutItemData, ApiLayoutItemFields);

    struct ApiLayoutData : virtual ApiResourceData {
        float cellAspectRatio;
        float cellSpacingWidth;
        float cellSpacingHeight;
        std::vector<ApiLayoutItem> items;
        bool   userCanEdit;
        bool   locked;
        QString backgroundImageFilename;
        qint32  backgroundWidth;
        qint32  backgroundHeight;
        float backgroundOpacity;
        qint32 userId;
    };

    #define ApiLayoutFields (cellAspectRatio) (cellSpacingWidth) (cellSpacingHeight) (items) (userCanEdit) (locked) (backgroundImageFilename) (backgroundWidth) (backgroundHeight) (backgroundOpacity) (userId)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiLayoutData, ApiResourceData, ApiLayoutFields);

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiLayout)

