    struct ApiData {
        virtual ~ApiData() {}
    };

    struct ApiIdData: public ApiData {
        NSUUID *id;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS(ApiIdData, (id))
