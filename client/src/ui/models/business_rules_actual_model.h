#ifndef __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
#define __QN_BUSINESS_RULES_ACTUAL_MODEL_H_

#include "business_rules_view_model.h"

#include <nx_ec/ec_api.h>

// TODO: #GDM move out from global namespace.
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

    typedef QnBusinessRulesViewModel base_type;
public:
    QnBusinessRulesActualModel(QObject *parent = 0);

    bool isLoaded() const;

signals:
    void beforeModelChanged();
    void afterModelChanged(QnBusinessRulesActualModelChange change, bool ok);
    
    void businessRuleChanged(QnId id);
    void businessRuleDeleted(QnId id);
public slots:
    /*
    * Load data from DB
    */
    void reloadData();

    void saveRule(int row);
private slots:
    void at_resources_received( int reqID, ec2::ErrorCode errorCode, const QnBusinessEventRuleList& rules );
    void at_resources_saved( int handle, ec2::ErrorCode errorCode, QnBusinessEventRulePtr rule );

    void at_message_ruleChanged(const QnBusinessEventRulePtr &rule);
    void at_message_ruleDeleted(QnId id);
    void at_message_ruleReset(QnBusinessEventRuleList rules);

private:
    int m_loadingHandle;
    bool m_isDataLoaded; // data sync with DB

    QMap<int, QnBusinessRuleViewModel*> m_savingRules;
};

#endif // __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
