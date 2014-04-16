    struct ApiPropertyTypeData: public ApiData {
        QnId resource_type_id;

        QString name;
        int type;

        // MinMaxStep
        qint32 min;
        qint32 max;
        qint32 step;

        // Enumaration
        QString values;
        QString ui_values;

        // Value
        QString default_value;

        QString group;
        QString sub_group;
        QString description;

        bool ui;
        bool readonly;

        QString netHelper;
    };

    #define ApiPropertyTypeFields (resource_type_id) (name) (type) (min) (max) (step) (values) (ui_values) (default_value) (group) (sub_group) (description) (ui) (readonly) (netHelper)
    QN_DEFINE_STRUCT_SERIALIZATORS (ApiPropertyTypeData, ApiPropertyTypeFields)

    struct ApiResourceTypeData : public ApiData {
        QnId id;
        QString name;
        QString manufacture;
        std::vector<QnId> parentId;
        std::vector<ApiPropertyType> propertyTypeList;
    };

    #define ApiResourceTypeFields (id) (name) (manufacture) (parentId) (propertyTypeList)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceTypeData, ApiResourceTypeFields);

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiResourceType)
