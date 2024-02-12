// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_rules_actual_model.h"

#include <api/common_message_processor.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <ui/workbench/workbench_context.h>

using namespace nx;

QnBusinessRulesActualModel::QnBusinessRulesActualModel(QObject *parent):
    QnBusinessRulesViewModel(parent)
{
    auto eventRuleManager = systemContext()->eventRuleManager();

    connect(eventRuleManager, &vms::event::RuleManager::ruleAddedOrUpdated,
        this, &QnBusinessRulesActualModel::at_ruleAddedOrUpdated);

    connect(eventRuleManager, &vms::event::RuleManager::ruleRemoved,
        this, &QnBusinessRulesActualModel::at_ruleRemoved);

    connect(eventRuleManager, &vms::event::RuleManager::rulesReset,
        this, &QnBusinessRulesActualModel::at_rulesReset);

    reset();
}

void QnBusinessRulesActualModel::reset()
{
    at_rulesReset(systemContext()->eventRuleManager()->rules());
}

void QnBusinessRulesActualModel::saveRule(const QModelIndex& index)
{
    QnBusinessRuleViewModelPtr ruleModel = rule(index);

    if (m_savingRules.values().contains(ruleModel))
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    vms::event::RulePtr rule = ruleModel->createRule();
    if (rule->id().isNull())
        rule->setId(nx::Uuid::createUuid());

    nx::vms::api::EventRuleData params;
    ec2::fromResourceToApi(rule, params);

    int handle = connection->getEventRulesManager(Qn::kSystemAccess)->save(
        params,
        [this, rule](int requestId, ec2::ErrorCode errorCode)
        {
            if (!m_savingRules.contains(requestId))
                return;
            QnBusinessRuleViewModelPtr model = m_savingRules[requestId];
            m_savingRules.remove(requestId);

            const bool success = errorCode == ec2::ErrorCode::ok;
            if(success)
                addOrUpdateRule(rule);
            emit afterModelChanged(RuleSaved, success);
        },
        this);
    m_savingRules[handle] = ruleModel;
}

void QnBusinessRulesActualModel::at_ruleAddedOrUpdated(const vms::event::RulePtr& rule)
{
    addOrUpdateRule(rule);
    emit eventRuleChanged(rule->id());
}

void QnBusinessRulesActualModel::at_ruleRemoved(const nx::Uuid& id)
{
    deleteRule(ruleModelById(id));  //< TODO: #sivanov Ask user.
    emit eventRuleDeleted(id);
}

void QnBusinessRulesActualModel::at_rulesReset(const vms::event::RuleList& rules)
{
    emit beforeModelChanged();
    clear();
    for (const auto& rule: rules)
        addOrUpdateRule(rule);
    emit afterModelChanged(RulesLoaded, true);
}
