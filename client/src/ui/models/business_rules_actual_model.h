#ifndef __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
#define __QN_BUSINESS_RULES_ACTUAL_MODEL_H_

#include "business_rules_view_model.h"

/*
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
    void afterModelChanged(int changeNum, int status);
    
    void businessRuleChanged(int id);
    void businessRuleDeleted(int id);
public slots:
    /*
    * Load data from DB
    */
    void reloadData();
private slots:
    void at_resources_received(int status, const QnBusinessEventRuleList &rules, int handle);

    void at_message_ruleChanged(const QnBusinessEventRulePtr &rule);
    void at_message_ruleDeleted(int id);

private:
    int m_loadingHandle;
    bool m_isDataLoaded; // data sync with DB
};

#endif // __QN_BUSINESS_RULES_ACTUAL_MODEL_H_
