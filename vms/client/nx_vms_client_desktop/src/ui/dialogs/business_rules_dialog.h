// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMenu>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/models/business_rules_actual_model.h>

namespace Ui {
class BusinessRulesDialog;
} // namespace Ui

class QnBusinessRuleWidget;

class QnBusinessRulesDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnBusinessRulesDialog(QWidget *parent);
    virtual ~QnBusinessRulesDialog();

    void setFilter(const QString &filter);

    /**
     * @brief tryClose      Checks if the dialog can be closed safely. If there are unsaved rules the user will be asked
     *                      and they will be saved or dropped.
     * @param force         If set to true, changes will be silently dropped and the dialog will be hidden.
     * @return              False if the user press Cancel, true otherwise.
     */
    virtual bool tryClose(bool force) override;
protected:
    virtual bool eventFilter(QObject *o, QEvent *e) override;
    virtual void keyPressEvent(QKeyEvent *event) override;

    virtual void showEvent(QShowEvent *event) override;

public:
    virtual void accept() override;
    virtual void reject() override;

private:
    void at_message_ruleDeleted(const QnUuid &id);

    void at_newRuleButton_clicked();
    void at_testRuleButton_clicked();
    void at_deleteButton_clicked();
    void at_resetDefaultsButton_clicked();

    void at_beforeModelChanged();
    void at_afterModelChanged(QnBusinessRulesActualModelChange change, bool ok);

    void at_tableView_currentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void at_tableViewport_resizeEvent();
    void at_model_dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

    void updateControlButtons();

    void updateFilter();

private:
    Q_DISABLE_COPY(QnBusinessRulesDialog);

    using Column = QnBusinessRuleViewModel::Column;

    void retranslateUi();
    void createActions();

    inline bool hasEditPermissions() const;
    inline bool hasChanges() const;

    bool saveAll();

    void testRule(const QnBusinessRuleViewModelPtr& ruleModel);
    void deleteRule(const QnBusinessRuleViewModelPtr& ruleModel);

    QScopedPointer<Ui::BusinessRulesDialog> ui;

    QnBusinessRulesActualModel* m_rulesViewModel;
    QList<QnUuid> m_pendingDeleteRules;

    QnBusinessRuleWidget* m_currentDetailsWidget;

    QMap<int, QnUuid> m_deleting;

    QMenu* m_popupMenu;

    QAction* m_newAction;
    QAction* m_deleteAction;

    QPushButton* m_resetDefaultsButton;
};
