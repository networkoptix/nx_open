    struct ApiStorageData: virtual ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64       spaceLimit;
        bool         usedForWriting;
    };

    #define ApiStorageFields  (spaceLimit) (usedForWriting)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiStorageData, ec2::ApiResourceData, ApiStorageFields);

    struct ApiMediaServerData: virtual ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        StorageVector storages;
    };

    #define medisServerDataFields (apiUrl) (netAddrList) (flags) (panicMode) (streamingUrl) (version) (authKey) (storages)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiMediaServerData, ec2::ApiResourceData, medisServerDataFields);
