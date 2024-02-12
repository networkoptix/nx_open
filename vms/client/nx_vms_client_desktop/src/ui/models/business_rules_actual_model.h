// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/rule.h>

#include "business_rules_view_model.h"

// TODO: #sivanov Move out from global namespace.
enum QnBusinessRulesActualModelChange {
    RulesLoaded,
    RuleSaved
};

/**
* Sync model data with actual DB data if DB was changed outside model
*/
class QnBusinessRulesActualModel: public QnBusinessRulesViewModel
{
    Q_OBJECT
    using base_type = QnBusinessRulesViewModel;

public:
    QnBusinessRulesActualModel(QObject* parent = nullptr);
    void reset();

signals:
    void beforeModelChanged();
    void afterModelChanged(QnBusinessRulesActualModelChange change, bool ok);

    void eventRuleChanged(const nx::Uuid& id);
    void eventRuleDeleted(const nx::Uuid& id);

public slots:
    void saveRule(const QModelIndex& index);

private slots:
    void at_ruleAddedOrUpdated(const nx::vms::event::RulePtr& rule);
    void at_ruleRemoved(const nx::Uuid& id);
    void at_rulesReset(const nx::vms::event::RuleList& rules);

private:
    QMap<int, QnBusinessRuleViewModelPtr> m_savingRules;
};
