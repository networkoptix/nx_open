#ifndef QN_BUSINESS_RULES_DIALOG_H
#define QN_BUSINESS_RULES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtCore/QModelIndex>

#include <QtWidgets/QDialog>
#include <QtWidgets/QMenu>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <api/api_fwd.h>

#include <business/business_event_rule.h>

#include <ui/dialogs/button_box_dialog.h>
#include <ui/models/business_rules_actual_model.h>
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
    explicit QnBusinessRulesDialog(QWidget *parent = 0);
    virtual ~QnBusinessRulesDialog();

    void setFilter(const QString &filter);

    /**
     * @brief canClose      Checks if the dialog can be closed safely. If there are unsaved rules the user will be asked
     *                      and they will be saved or dropped.
     * @return              False if the user press Cancel, true otherwise.
     */
    bool canClose();
protected:
    virtual bool eventFilter(QObject *o, QEvent *e) override;
    virtual void keyPressEvent(QKeyEvent *event) override;

public Q_SLOTS:
    virtual void accept() override;
    virtual void reject() override;

private slots:
    void at_message_ruleDeleted(const QUuid &id);

    void at_newRuleButton_clicked();
    void at_saveAllButton_clicked();
    void at_deleteButton_clicked();
    void at_resetDefaultsButton_clicked();
    void at_clearFilterButton_clicked();

    void at_beforeModelChanged();
    void at_afterModelChanged(QnBusinessRulesActualModelChange change, bool ok);

    void at_resources_deleted( int handle, ec2::ErrorCode errorCode );

    void at_tableView_currentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void at_tableViewport_resizeEvent();
    void at_model_dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);


    void toggleAdvancedMode();
    void updateAdvancedAction();
    void updateControlButtons();

    void updateFilter();

private:
    Q_DISABLE_COPY(QnBusinessRulesDialog)

    void createActions();

    bool saveAll();

    void deleteRule(QnBusinessRuleViewModel* ruleModel);

    bool advancedMode() const;
    void setAdvancedMode(bool value);

    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QnBusinessRulesActualModel* m_rulesViewModel;
    QList<QnId> m_pendingDeleteRules;

    QnBusinessRuleWidget* m_currentDetailsWidget;


    QMap<int, QnId> m_deleting;

    QMenu* m_popupMenu;

    QAction* m_newAction;
    QAction* m_deleteAction;
    QAction* m_advancedAction;

    QPushButton* m_resetDefaultsButton;

    bool m_advancedMode;
};

#endif // QN_BUSINESS_RULES_DIALOG_H
