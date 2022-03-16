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

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    void setupRuleTableView();
    void updateReadOnlyState();
    void showTipPanel(bool show);
    void displayRule();
    void displayEvent(const RulesTableModel::SimplifiedRule& rule);
    void displayAction(const RulesTableModel::SimplifiedRule& rule);
    void displayComment(const RulesTableModel::SimplifiedRule& rule);
    void createEventEditor(const vms::rules::ItemDescriptor& descriptor);
    void createActionEditor(const vms::rules::ItemDescriptor& descriptor);
    void resetFilter();
    void resetSelection();
    void updateRule();
    void onModelDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles);

private:
    std::unique_ptr<Ui::RulesDialog> ui;

    bool readOnly{};
    ParamsWidget* eventEditorWidget{};
    ParamsWidget* actionEditorWidget{};
    RulesTableModel* rulesTableModel{};
    QSortFilterProxyModel* rulesFilterModel{};
    std::weak_ptr<RulesTableModel::SimplifiedRule> displayedRule;
};

} // namespace nx::vms::client::desktop::rules
