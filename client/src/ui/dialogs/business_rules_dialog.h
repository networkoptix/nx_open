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
#include <ui/dialogs/button_box_dialog.h>
#include <ui/widgets/business/business_rule_widget.h>
#include <utils/common/request_param.h>



namespace Ui {
    class BusinessRulesDialog;
}

class QnBusinessRulesDialog : public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent = 0);
    virtual ~QnBusinessRulesDialog();

protected:
    virtual bool eventFilter(QObject *o, QEvent *e) override;

private slots:
    void at_newRuleButton_clicked();
    void at_saveButton_clicked();
    void at_saveAllButton_clicked();
    void at_deleteButton_clicked();

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
    QList<QStandardItem *> createRow(QnBusinessRuleWidget* widget);
    QnBusinessRuleWidget* createWidget(QnBusinessEventRulePtr rule);

    void saveRule(QnBusinessRuleWidget* widget);
    void deleteRule(QnBusinessRuleWidget* widget);

    QStandardItem *tableItem(QnBusinessRuleWidget* widget, int column) const;

    void updateControlButtons();

    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QStandardItemModel* m_listModel;
    QnBusinessRuleWidget* m_currentDetailsWidget;

    QMap<int, QnBusinessRuleWidget*> m_processingWidgets;
    QnAppServerConnectionPtr m_connection;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
