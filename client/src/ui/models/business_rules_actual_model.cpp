#include "business_rules_actual_model.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include "ui/workbench/workbench_context.h"

QnBusinessRulesActualModel::QnBusinessRulesActualModel(QObject *parent): 
    QnBusinessRulesViewModel(parent),
    m_loadingHandle(-1),
    m_isDataLoaded(false)
{
    connect(context(),  SIGNAL(userChanged(const QnUserResourcePtr &)),          
            this, SLOT(reloadData()));
    connect(QnCommonMessageProcessor::instance(),           SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)),
            this, SLOT(at_message_ruleChanged(QnBusinessEventRulePtr)));
    connect(QnCommonMessageProcessor::instance(),           SIGNAL(businessRuleDeleted(QnId)),
            this, SLOT(at_message_ruleDeleted(QnId)));
    connect(QnCommonMessageProcessor::instance(),           SIGNAL(businessRuleReset(QnBusinessEventRuleList)),
            this, SLOT(at_message_ruleReset(QnBusinessEventRuleList)));

}

void QnBusinessRulesActualModel::reloadData() 
{
    clear();
    m_isDataLoaded = false;
    m_loadingHandle = QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->getBusinessRules(
        this, &QnBusinessRulesActualModel::at_resources_received );
    m_savingRules.clear();
    emit beforeModelChanged();
}

void QnBusinessRulesActualModel::saveRule(int row) {
    QnBusinessRuleViewModel* ruleModel = getRuleModel(row);

    if (m_savingRules.values().contains(ruleModel))
        return;

    QnBusinessEventRulePtr rule = ruleModel->createRule();
    using namespace std::placeholders;
    int handle = QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->save(
        rule, this, [this, rule]( int handle, ec2::ErrorCode errorCode ){ at_resources_saved( handle, errorCode, rule ); } );
    m_savingRules[handle] = ruleModel;
}

void QnBusinessRulesActualModel::at_resources_received( int handle, ec2::ErrorCode errorCode, const QnBusinessEventRuleList& rules )
{
    if (handle != m_loadingHandle)
        return;
    m_isDataLoaded = errorCode == ec2::ErrorCode::ok;
    addRules(rules);
    emit afterModelChanged(RulesLoaded, m_isDataLoaded);
    m_loadingHandle = -1;
}

void QnBusinessRulesActualModel::at_resources_saved( int handle, ec2::ErrorCode errorCode, QnBusinessEventRulePtr rule ) {
    if (!m_savingRules.contains(handle))
        return;
    QnBusinessRuleViewModel* model = m_savingRules[handle];
    m_savingRules.remove(handle);

    const bool success = errorCode == ec2::ErrorCode::ok;
    if(success) {
        if (model->id() == 0)
            deleteRule(model);
        updateRule(rule);
    }
    emit afterModelChanged(RuleSaved, success);
}

void QnBusinessRulesActualModel::at_message_ruleChanged(const QnBusinessEventRulePtr &rule) {
    updateRule(rule);
    emit businessRuleChanged(rule->id());
}

void QnBusinessRulesActualModel::at_message_ruleDeleted(QnId id) {
    deleteRule(id);  //TODO: #GDM ask user
    emit businessRuleDeleted(id);
}

void QnBusinessRulesActualModel::at_message_ruleReset(QnBusinessEventRuleList rules) {
    emit beforeModelChanged();
    clear();
    foreach (const QnBusinessEventRulePtr &rule, rules)
        addRule(rule);
    emit afterModelChanged(RulesLoaded, true);
}

bool QnBusinessRulesActualModel::isLoaded() const
{
    return m_isDataLoaded;
}

