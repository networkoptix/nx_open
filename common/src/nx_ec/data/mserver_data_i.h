    struct ApiStorageData: virtual ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64       spaceLimit;
        bool         usedForWriting;
    };

    #define ApiStorageFields  (spaceLimit) (usedForWriting)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiStorageData, ApiResourceData, ApiStorageFields);

    struct ApiMediaServerData: virtual ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      systemInfo;
        QString      authKey;
        std::vector<ApiStorage> storages;
    };

    #define medisServerDataFields (apiUrl) (netAddrList) (flags) (panicMode) (streamingUrl) (version) (systemInfo) (authKey) (storages)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiMediaServerData, ApiResourceData, medisServerDataFields);

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiMediaServer)
