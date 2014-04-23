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

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiLayoutItemFields (uuid) (flags) (left) (top) (right) (bottom) (rotation) (resourceId) (resourcePath) (zoomLeft) (zoomTop) (zoomRight) (zoomBottom) (zoomTargetUuid) (contrastParams) (dewarpingParams)
    //QN_DEFINE_STRUCT_SERIALIZATORS(ApiLayoutItemData, ApiLayoutItemFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiLayoutItemData, (binary))

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

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiLayoutFields (cellAspectRatio) (cellSpacingWidth) (cellSpacingHeight) (items) (userCanEdit) (locked) (backgroundImageFilename) (backgroundWidth) (backgroundHeight) (backgroundOpacity) (userId)
    //QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiLayoutData, ApiResourceData, ApiLayoutFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiLayoutData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiLayoutData);
