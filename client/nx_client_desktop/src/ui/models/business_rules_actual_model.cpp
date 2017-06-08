#include "business_rules_actual_model.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include <business/event_rule_manager.h>
#include <common/common_module.h>

#include <ui/workbench/workbench_context.h>

QnBusinessRulesActualModel::QnBusinessRulesActualModel(QObject *parent):
    QnBusinessRulesViewModel(parent)
{
    auto eventRuleManager = commonModule()->eventRuleManager();

    connect(eventRuleManager, &QnEventRuleManager::ruleAddedOrUpdated,
        this, &QnBusinessRulesActualModel::at_ruleAddedOrUpdated);

    connect(eventRuleManager, &QnEventRuleManager::ruleRemoved,
        this, &QnBusinessRulesActualModel::at_ruleRemoved);

    connect(eventRuleManager, &QnEventRuleManager::rulesReset,
        this, &QnBusinessRulesActualModel::at_rulesReset);

    reset();
}

void QnBusinessRulesActualModel::reset()
{
    at_rulesReset(commonModule()->eventRuleManager()->rules());
}

void QnBusinessRulesActualModel::saveRule(const QModelIndex& index)
{
    QnBusinessRuleViewModelPtr ruleModel = rule(index);

    if (m_savingRules.values().contains(ruleModel))
        return;

    if (!commonModule()->ec2Connection())
        return;

    QnBusinessEventRulePtr rule = ruleModel->createRule();

    //TODO: #GDM SafeMode
    int handle = commonModule()->ec2Connection()->getBusinessEventManager(Qn::kSystemAccess)->save(
        rule, this, [this, rule]( int handle, ec2::ErrorCode errorCode ){ at_resources_saved( handle, errorCode, rule ); } );
    m_savingRules[handle] = ruleModel;
}

void QnBusinessRulesActualModel::at_resources_saved(int handle, ec2::ErrorCode errorCode, const QnBusinessEventRulePtr& rule)
{
    if (!m_savingRules.contains(handle))
        return;
    QnBusinessRuleViewModelPtr model = m_savingRules[handle];
    m_savingRules.remove(handle);

    const bool success = errorCode == ec2::ErrorCode::ok;
    if(success)
        addOrUpdateRule(rule);
    emit afterModelChanged(RuleSaved, success);
}

void QnBusinessRulesActualModel::at_ruleAddedOrUpdated(const QnBusinessEventRulePtr& rule)
{
    addOrUpdateRule(rule);
    emit eventRuleChanged(rule->id());
}

void QnBusinessRulesActualModel::at_ruleRemoved(const QnUuid& id)
{
    deleteRule(ruleModelById(id));  //TODO: #GDM #Business ask user
    emit eventRuleDeleted(id);
}

void QnBusinessRulesActualModel::at_rulesReset(const QnBusinessEventRuleList& rules)
{
    emit beforeModelChanged();
    clear();
    for (const auto& rule: rules)
        addOrUpdateRule(rule);
    emit afterModelChanged(RulesLoaded, true);
}
