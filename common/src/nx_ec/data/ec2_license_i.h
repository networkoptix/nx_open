    struct ApiLicenseData : ApiData {
        QByteArray key;
        QByteArray licenseBlock;

        ApiLicenseData() {}
    };

    #define ApiLicenseFields (key)(licenseBlock)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiLicenseData, ApiLicenseFields)

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiLicense)