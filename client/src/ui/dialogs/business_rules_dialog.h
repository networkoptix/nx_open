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

#include <ui/models/business_rules_view_model.h>

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
    explicit QnBusinessRulesDialog(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
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
    void at_advancedButton_clicked();

    void at_resources_received(int status, const QByteArray& errorString, const QnBusinessEventRules &rules, int handle);
    void at_resources_saved(int status, const QByteArray& errorString, const QnBusinessEventRules &rules, int handle);
    void at_resources_deleted(const QnHTTPRawResponse& response, int handle);

    void at_tableView_currentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void at_model_dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
private:
    Q_DISABLE_COPY(QnBusinessRulesDialog)

    void saveRule(QnBusinessRuleViewModel* ruleModel);
    void deleteRule(QnBusinessRuleViewModel* ruleModel);

    void updateControlButtons();

    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QnBusinessRulesViewModel* m_rulesViewModel;

    QnBusinessRuleWidget* m_currentDetailsWidget;

    QMap<int, QnBusinessRuleViewModel*> m_processing;
    int m_loadingHandle;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
