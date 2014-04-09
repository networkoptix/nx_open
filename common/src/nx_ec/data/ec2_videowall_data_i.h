    struct ApiVideowallData: virtual ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}
    
        bool autorun;
    };

    #define ApiVideowallDataFields (autorun)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiVideowallData, ec2::ApiResourceData, ApiVideowallDataFields);

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiVideowall)
