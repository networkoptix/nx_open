    struct ApiBusinessRuleData: ApiData {
        ApiBusinessRuleData(): 
            eventType(QnBusiness::UndefinedEvent), eventState(Qn::UndefinedState), actionType(QnBusiness::UndefinedAction), 
            aggregationPeriod(0), disabled(false), system(false) {}

        QnId id;

        QnBusiness::EventType eventType;
        std::vector<QnId>  eventResource;
        QByteArray eventCondition;
        Qn::ToggleState eventState;
    
        QnBusiness::ActionType actionType;
        std::vector<QnId> actionResource;
        QByteArray actionParams;

        qint32 aggregationPeriod; // msecs
        bool disabled;
        QString comments;
        QString schedule;

        bool system; // system rule cannot be deleted 
    };

    #define ApiBusinessRuleFields (id) (eventType) (eventResource) (eventCondition) (eventState) (actionType) (actionResource) (actionParams) (aggregationPeriod) (disabled) (comments) (schedule) (system)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiBusinessRuleData, ApiBusinessRuleFields);

    QN_DEFINE_API_OBJECT_LIST_DATA(ApiBusinessRule)