#ifndef QN_BUSINESS_RULES_DIALOG_H
#define QN_BUSINESS_RULES_DIALOG_H

#include <QtCore/QScopedPointer>

#include <QtGui/QDialog>
#include <QtCore/QModelIndex>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <api/api_fwd.h>

#include <core/resource/resource_fwd.h>
#include <events/business_event_rule.h>
#include <ui/widgets/business/business_rule_widget.h>
#include <utils/common/request_param.h>



namespace Ui {
    class BusinessRulesDialog;
}

class QnBusinessRulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent = 0);
    virtual ~QnBusinessRulesDialog();

private slots:
    void at_newRuleButton_clicked();

    void at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle);
    void at_resources_deleted(const QnHTTPRawResponse& response, int handle);

    /* Widget changes handlers */
    void at_ruleHasChangesChanged(QnBusinessRuleWidget* source, bool value);
    void at_ruleEventTypeChanged(QnBusinessRuleWidget* source, BusinessEventType::Value value);
    void at_ruleEventResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource);
    void at_ruleEventStateChanged(QnBusinessRuleWidget* source, ToggleState::Value value);
    void at_ruleActionTypeChanged(QnBusinessRuleWidget* source, BusinessActionType::Value value);
    void at_ruleActionResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource);



    void at_tableView_currentRowChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    QList<QStandardItem *> itemFromRule(QnBusinessEventRulePtr rule);

    void addRuleToList(QnBusinessEventRulePtr rule);
    void saveRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);
    void deleteRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);

    QStandardItem *tableItem(QnBusinessRuleWidget* widget, int column) const;


    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QStandardItemModel* m_listModel;
    QnBusinessRuleWidget* m_currentDetailsWidget;

    //QHash<QString, QnBusinessRuleWidget*> m_ruleWidgets;

    QnAppServerConnectionPtr m_connection;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
