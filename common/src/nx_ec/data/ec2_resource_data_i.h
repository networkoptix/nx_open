    struct ApiResourceParamData: public ApiData
    {
        ApiResourceParamData() {}
        ApiResourceParamData(const QString& name, const QString& value, bool isResTypeParam): value(value), name(name), isResTypeParam(isResTypeParam) {}

        QString value;
        QString name;
        bool isResTypeParam;
    };

    #define ApiResourceParamFields (value) (name) (isResTypeParam)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceParamData, ApiResourceParamFields);

    struct ApiResourceData: virtual ApiData {
        ApiResourceData(): status(QnResource::Offline), disabled(false) {}

        QnId          id;
        QnId          parentGuid;
        QnResource::Status    status;
        bool          disabled;
        QString       name;
        QString       url;
        QnId          typeId;
        ApiResourceParamVector addParams;
    };

    #define ApiResourceFields (id) (parentGuid) (status) (disabled) (name) (url) (typeId) (addParams)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceData,  ApiResourceFields)
