#ifndef __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
#define __QN_BUSINESS_RULES_ACTUAL_MODEL_H_

#include "business_rules_view_model.h"

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
public:
    QnBusinessRulesActualModel(QObject *parent = 0);

    bool isLoaded() const;
signals:
    void beforeModelChanged(int changeNum);
    void afterModelChanged(QnBusinessRulesActualModelChange change, bool ok);
    
    void businessRuleChanged(int id);
    void businessRuleDeleted(int id);
public slots:
    /*
    * Load data from DB
    */
    void reloadData();

    void saveRule(int row);
private slots:
    void at_resources_received(int status, const QnBusinessEventRuleList &rules, int handle);
    void at_resources_saved(int status, const QnBusinessEventRuleList &rules, int handle);

    void at_message_ruleChanged(const QnBusinessEventRulePtr &rule);
    void at_message_ruleDeleted(int id);

private:
    int m_loadingHandle;
    bool m_isDataLoaded; // data sync with DB

    QMap<int, QnBusinessRuleViewModel*> m_savingRules;
};

#endif // __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
