    struct ApiData {
        virtual ~ApiData() {}
    };

    struct ApiIdData: public ApiData {
        QnId id;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS(ApiIdData, (id))
