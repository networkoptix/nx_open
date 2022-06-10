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

private:
    void setupRuleTableView();
    void updateReadOnlyState();
    void updateControlButtons();
    void updateRuleEditorPanel();
    void displayRule();
    void displayEvent(const SimplifiedRule& rule);
    void displayAction(const SimplifiedRule& rule);
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

private:
    std::unique_ptr<Ui::RulesDialog> ui;

    bool readOnly{};
    ParamsWidget* eventEditorWidget{};
    ParamsWidget* actionEditorWidget{};
    RulesTableModel* rulesTableModel{};
    QSortFilterProxyModel* rulesFilterModel{};
    std::weak_ptr<SimplifiedRule> displayedRule;
};

} // namespace nx::vms::client::desktop::rules
