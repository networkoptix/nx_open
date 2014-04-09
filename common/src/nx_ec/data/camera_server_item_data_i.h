    struct ApiCameraServerItemData : ApiData {
        ApiCameraServerItemData(): timestamp(0) {}

        QString  physicalId;
        QString  serverGuid;
        qint64   timestamp;
    };

    #define ApiCameraServerItemFields (physicalId) (serverGuid) (timestamp)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiCameraServerItemData, ApiCameraServerItemFields);

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiCameraServerItem)