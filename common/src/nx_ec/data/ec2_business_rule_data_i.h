    struct ApiBusinessRuleData: ApiData {
        ApiBusinessRuleData(): 
            eventType(BusinessEventType::NotDefined), eventState(Qn::UndefinedState), actionType(BusinessActionType::NotDefined), 
            aggregationPeriod(0), disabled(false), system(false) {}

        QnId id;

        BusinessEventType::Value eventType;
        std::vector<QnId>  eventResource;
        QByteArray eventCondition;
        Qn::ToggleState eventState;
    
        BusinessActionType::Value actionType;
        std::vector<QnId> actionResource;
        QByteArray actionParams;

        qint32 aggregationPeriod; // msecs
        bool disabled;
        QString comments;
        QString schedule;

        bool system; // system rule cannot be deleted 
    };

    #define ApiBusinessRuleFields (id) (eventType) (eventResource) (eventCondition) (eventState) (actionType) (actionResource) (actionParams) (aggregationPeriod) (disabled) (comments) (schedule) (system)
    //QN_DEFINE_STRUCT_SERIALIZATORS(ApiBusinessRuleData, ApiBusinessRuleFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiBusinessRuleData, (binary))

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiBusinessRule)
    