    struct ApiUserData : virtual ApiResourceData {
        ApiUserData(): isAdmin(false), rights(0) {}
    
        //QString password;
        bool isAdmin;
        qint64 rights;
        QString email;
        QByteArray digest;
        QByteArray hash; 
    };

    #define ApiUserFields (isAdmin) (rights) (email) (digest) (hash)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiUserData, ec2::ApiResourceData, ApiUserFields);
