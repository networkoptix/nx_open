// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <ui/dialogs/common/session_aware_dialog.h>

#include "model_view/rules_table_model.h"

class QSortFilterProxyModel;

namespace Ui { class RulesDialog; }

namespace nx::vms::client::desktop::rules {

class ParamsWidget;

class RulesDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

public:
    explicit RulesDialog(QWidget* parent = nullptr);
    virtual ~RulesDialog() override;

    virtual bool tryClose(bool force) override;
    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    void setupRuleTableView();
    void updateReadOnlyState();
    void updateControlButtons();
    void updateRuleEditorPanel();
    void displayRule();
    void displayEvent(const std::shared_ptr<SimplifiedRule>& rule);
    void displayAction(const std::shared_ptr<SimplifiedRule>& rule);
    void createEventEditor(const vms::rules::ItemDescriptor& descriptor);
    void createActionEditor(const vms::rules::ItemDescriptor& descriptor);
    void resetFilter();
    void resetSelection();
    void onModelDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles);
    void applyChanges();
    void rejectChanges();
    void resetToDefaults();
    void deleteCurrentRule();

private:
    std::unique_ptr<Ui::RulesDialog> m_ui;

    bool m_readOnly{false};
    QPointer<ParamsWidget> m_eventEditorWidget;
    QPointer<ParamsWidget> m_actionEditorWidget;
    RulesTableModel* m_rulesTableModel{nullptr};
    QSortFilterProxyModel* m_rulesFilterModel{nullptr};
    std::weak_ptr<SimplifiedRule> m_displayedRule;
};

} // namespace nx::vms::client::desktop::rules
