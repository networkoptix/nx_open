#include "business_rules_actual_model.h"
#include "api/app_server_connection.h"
#include "client_message_processor.h"
#include "ui/workbench/workbench_context.h"

QnBusinessRulesActualModel::QnBusinessRulesActualModel(QObject *parent): 
    QnBusinessRulesViewModel(parent),
    m_loadingHandle(-1),
    m_isDataLoaded(false)
{
    connect(context(),  SIGNAL(userChanged(const QnUserResourcePtr &)),          
            this, SLOT(at_context_userChanged()));
    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)),
            this, SLOT(at_message_ruleChanged(QnBusinessEventRulePtr)));
    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleDeleted(int)),
            this, SLOT(at_message_ruleDeleted(int)));

}

void QnBusinessRulesActualModel::reloadData()
{
    at_context_userChanged();
}

void QnBusinessRulesActualModel::at_context_userChanged() 
{
    clear();
    m_isDataLoaded = false;
    m_loadingHandle = QnAppServerConnectionFactory::createConnection()->getBusinessRulesAsync(
        this, SLOT(at_resources_received(int,QnBusinessEventRuleList,int)));
    emit beforeModelChanged(m_loadingHandle);
}

void QnBusinessRulesActualModel::at_resources_received(int status, const QnBusinessEventRuleList &rules, int handle) {
    if (handle != m_loadingHandle)
        return;
    addRules(rules);
    m_isDataLoaded = status == 0;
    emit afterModelChanged(m_loadingHandle, status);
    m_loadingHandle = -1;
}

void QnBusinessRulesActualModel::at_message_ruleChanged(const QnBusinessEventRulePtr &rule) {
    updateRule(rule);
    emit businessRuleChanged(rule->id());
}

void QnBusinessRulesActualModel::at_message_ruleDeleted(int id) {
    deleteRule(id);  //TODO: #GDM ask user
    emit businessRuleDeleted(id);
}

bool QnBusinessRulesActualModel::isLoaded() const
{
    return m_isDataLoaded;
}
