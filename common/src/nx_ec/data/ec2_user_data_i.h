    struct ApiUserData : ApiResourceData {
        ApiUserData(): isAdmin(false), rights(0) {}
    
        //QString password;
        bool isAdmin;
        qint64 rights;
        QString email;
        QByteArray digest;
        QByteArray hash; 

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiUserFields (isAdmin) (rights) (email) (digest) (hash)
    //QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiUserData, ApiResourceData, ApiUserFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiUserData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiUserData)
