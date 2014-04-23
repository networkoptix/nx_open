    struct ApiStorageData: ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64       spaceLimit;
        bool         usedForWriting;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiStorageFields  (spaceLimit) (usedForWriting)
    //QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiStorageData, ApiResourceData, ApiStorageFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiStorageData, (binary))

    struct ApiMediaServerData: ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0), maxCameras(0), redundancy(false) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        std::vector<ApiStorageData> storages;
        int maxCameras;
        bool redundancy;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define medisServerDataFields (apiUrl) (netAddrList) (flags) (panicMode) (streamingUrl) (version) (authKey) (storages) (maxCameras) (redundancy)
    //QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiMediaServerData, ApiResourceData, medisServerDataFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiMediaServerData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiMediaServerData)
	
