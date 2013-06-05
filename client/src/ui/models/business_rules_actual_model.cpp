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
            this, SLOT(reloadData()));
    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)),
            this, SLOT(at_message_ruleChanged(QnBusinessEventRulePtr)));
    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleDeleted(int)),
            this, SLOT(at_message_ruleDeleted(int)));

}

void QnBusinessRulesActualModel::reloadData() 
{
    clear();
    m_isDataLoaded = false;
    m_loadingHandle = QnAppServerConnectionFactory::createConnection()->getBusinessRulesAsync(
        this, SLOT(at_resources_received(int,QnBusinessEventRuleList,int)));
    m_savingRules.clear();
    emit beforeModelChanged();
}

void QnBusinessRulesActualModel::saveRule(int row) {
    QnBusinessRuleViewModel* ruleModel = getRuleModel(row);

    if (m_savingRules.values().contains(ruleModel))
        return;

    QnBusinessEventRulePtr rule = ruleModel->createRule();
    int handle = QnAppServerConnectionFactory::createConnection()->saveAsync(
                rule, this, SLOT(at_resources_saved(int, const QnBusinessEventRuleList &, int)));
    m_savingRules[handle] = ruleModel;
}

void QnBusinessRulesActualModel::at_resources_received(int status, const QnBusinessEventRuleList &rules, int handle) {
    if (handle != m_loadingHandle)
        return;
    m_isDataLoaded = status == 0;
    addRules(rules);
    emit afterModelChanged(RulesLoaded, m_isDataLoaded);
    m_loadingHandle = -1;
}

void QnBusinessRulesActualModel::at_resources_saved(int status, const QnBusinessEventRuleList &rules, int handle) {
    if (!m_savingRules.contains(handle))
        return;
    QnBusinessRuleViewModel* model = m_savingRules[handle];
    m_savingRules.remove(handle);

    bool success = (status == 0 && rules.size() == 1);
    if(success) {
        if (model->id() == 0)
            deleteRule(model);
        updateRule(rules.first());
    }
    emit afterModelChanged(RuleSaved, success);
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
