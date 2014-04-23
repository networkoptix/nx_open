    struct ApiResourceParamData: ApiData
    {
        ApiResourceParamData() {}
        ApiResourceParamData(const QString& name, const QString& value, bool isResTypeParam): value(value), name(name), isResTypeParam(isResTypeParam) {}

        QString value;
        QString name;
        bool isResTypeParam;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiResourceParamFields (value) (name) (isResTypeParam)
    //QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceParamData, ApiResourceParamFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiResourceParamData, (binary))

    struct ApiResourceData: ApiData {
        ApiResourceData(): status(QnResource::Offline), disabled(false) {}

        QnId          id;
        QnId          parentGuid;
        QnResource::Status    status;
        bool          disabled;
        QString       name;
        QString       url;
        QnId          typeId;
        std::vector<ApiResourceParamData> addParams;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiResourceFields (id) (parentGuid) (status) (disabled) (name) (url) (typeId) (addParams)
    //QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceData,  ApiResourceFields)
    QN_FUSION_DECLARE_FUNCTIONS(ApiResourceData, (binary))
