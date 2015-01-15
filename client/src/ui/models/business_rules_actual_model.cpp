#include "business_rules_actual_model.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include "ui/workbench/workbench_context.h"

QnBusinessRulesActualModel::QnBusinessRulesActualModel(QObject *parent): 
    QnBusinessRulesViewModel(parent)
{
    connect(QnCommonMessageProcessor::instance(),           &QnCommonMessageProcessor::businessRuleChanged, this, &QnBusinessRulesActualModel::at_message_ruleChanged);
    connect(QnCommonMessageProcessor::instance(),           &QnCommonMessageProcessor::businessRuleDeleted, this, &QnBusinessRulesActualModel::at_message_ruleDeleted);
    connect(QnCommonMessageProcessor::instance(),           &QnCommonMessageProcessor::businessRuleReset,   this, &QnBusinessRulesActualModel::at_message_ruleReset);

    reset();
}

void QnBusinessRulesActualModel::reset() {
    at_message_ruleReset(QnCommonMessageProcessor::instance()->businessRules().values());
}

void QnBusinessRulesActualModel::saveRule(int row) {
    QnBusinessRuleViewModel* ruleModel = getRuleModel(row);

    if (m_savingRules.values().contains(ruleModel))
        return;

    if (!QnAppServerConnectionFactory::getConnection2())
        return;

    QnBusinessEventRulePtr rule = ruleModel->createRule();
    //using namespace std::placeholders;
    int handle = QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->save(
        rule, this, [this, rule]( int handle, ec2::ErrorCode errorCode ){ at_resources_saved( handle, errorCode, rule ); } );
    m_savingRules[handle] = ruleModel;
}

void QnBusinessRulesActualModel::at_resources_saved( int handle, ec2::ErrorCode errorCode, const QnBusinessEventRulePtr &rule) {
    if (!m_savingRules.contains(handle))
        return;
    QnBusinessRuleViewModel* model = m_savingRules[handle];
    m_savingRules.remove(handle);

    const bool success = errorCode == ec2::ErrorCode::ok;
    if(success) {
        if (model->id().isNull())
            deleteRule(model);
        addOrUpdateRule(rule);
    }
    emit afterModelChanged(RuleSaved, success);
}

void QnBusinessRulesActualModel::at_message_ruleChanged(const QnBusinessEventRulePtr &rule) {
    addOrUpdateRule(rule);
    emit businessRuleChanged(rule->id());
}

void QnBusinessRulesActualModel::at_message_ruleDeleted(const QnUuid &id) {
    deleteRule(id);  //TODO: #GDM #Business ask user
    emit businessRuleDeleted(id);
}

void QnBusinessRulesActualModel::at_message_ruleReset(const QnBusinessEventRuleList &rules) {
    emit beforeModelChanged();
    clear();
    QnBusinessEventRuleList sorted(rules);
    qSort(sorted.begin(), sorted.end(), [](const QnBusinessEventRulePtr &left, const QnBusinessEventRulePtr &right) {
        if (left->isDisabled() != right->isDisabled())
            return right->isDisabled();

        if (left->eventType() != right->eventType())
            return left->eventType() < right->eventType();

        if (left->actionType() != right->actionType())
            return left->actionType() < right->actionType();

        if (left->eventResources().size() != right->eventResources().size())
            return (left->eventResources().size() < right->eventResources().size());

        if (left->actionResources().size() != right->actionResources().size())
            return (left->actionResources().size() < right->actionResources().size());        

        return left->id() < right->id();
    });

    for (const QnBusinessEventRulePtr &rule: sorted)
        addOrUpdateRule(rule);
    emit afterModelChanged(RulesLoaded, true);
}

