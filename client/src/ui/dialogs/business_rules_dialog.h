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

    void at_tableView_currentRowChanged(const QModelIndex& current, const QModelIndex& previous);

    void saveRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);
    void deleteRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule);
private:
    QList<QStandardItem *> itemFromRule(QnBusinessEventRulePtr rule, int row = -1);

    void addRuleToList(QnBusinessEventRulePtr rule);

    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QStandardItemModel* m_listModel;
    QStandardItem* m_newRuleItem;
    QnBusinessEventRulePtr m_deletingRule;
    QnBusinessEventRules m_rules;

    QnAppServerConnectionPtr m_connection;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
