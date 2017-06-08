#ifndef __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
#define __QN_BUSINESS_RULES_ACTUAL_MODEL_H_

#include "business_rules_view_model.h"

#include <nx_ec/ec_api.h>

// TODO: #GDM #Business move out from global namespace.
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

    void eventRuleChanged(const QnUuid& id);
    void eventRuleDeleted(const QnUuid& id);

public slots:
    void saveRule(const QModelIndex& index);

private slots:
    void at_resources_saved(int handle, ec2::ErrorCode errorCode, const QnBusinessEventRulePtr& rule);

    void at_ruleAddedOrUpdated(const QnBusinessEventRulePtr& rule);
    void at_ruleRemoved(const QnUuid& id);
    void at_rulesReset(const QnBusinessEventRuleList& rules);

private:
    QMap<int, QnBusinessRuleViewModelPtr> m_savingRules;
};

#endif // __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
