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
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/request_param.h>

namespace Ui {
    class BusinessRulesDialog;
}

class QnBusinessRulesDialog : public QnButtonBoxDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    virtual ~QnBusinessRulesDialog();

protected:
    virtual bool eventFilter(QObject *o, QEvent *e) override;

private slots:
    void at_context_userChanged();

    void at_message_ruleChanged(const QnBusinessEventRulePtr &rule);
    void at_message_ruleDeleted(QnId id);

    void at_newRuleButton_clicked();
    void at_saveAllButton_clicked();
    void at_deleteButton_clicked();

    void at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle);
    void at_resources_deleted(const QnHTTPRawResponse& response, int handle);

    /* Widget changes handlers */
    void at_widgetHasChangesChanged(QnBusinessRuleWidget* source, bool value);
    void at_widgetDefinitionChanged(QnBusinessRuleWidget* source,
                                    BusinessEventType::Value eventType,
                                    ToggleState::Value eventState,
                                    BusinessActionType::Value actionType);

    void at_widgetEventResourcesChanged(QnBusinessRuleWidget* source, BusinessEventType::Value eventType, const QnResourceList &resources);
    void at_widgetActionResourcesChanged(QnBusinessRuleWidget* source, BusinessActionType::Value actionType, const QnResourceList &resources);

    void at_tableView_currentRowChanged(const QModelIndex& current, const QModelIndex& previous);
private:
    Q_DISABLE_COPY(QnBusinessRulesDialog)

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
