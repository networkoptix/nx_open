    struct ApiResourceData: public ApiData {
        ApiResourceData(): status(QnResource::Offline), disabled(false) {}

        QnId          id;
        QnId          parentGuid;
        QnResource::Status    status;
        bool          disabled;
        QString       name;
        QString       url;
        QnId          typeId;
        std::vector<ApiResourceParam> addParams;
    };
    #define ApiResourceFields (id) (parentGuid) (status) (disabled) (name) (url) (typeId) (addParams)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceData,  ApiResourceFields)
