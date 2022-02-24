// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_set>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>

namespace Ui { class VmsRulesDialog; }

namespace nx::vms::client::desktop::entity_item_model { class EntityItemModel; }

namespace nx::vms::client::desktop {
namespace vms_rules {

class VmsRulesItemDelegate;
class VmsRuleEditorWindow;
class VmsRulesFilterModel;

class VmsRulesDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    VmsRulesDialog(QWidget* parent = nullptr);
    virtual ~VmsRulesDialog() override;

private:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    void setupRuleListView();

private:
    QScopedPointer<Ui::VmsRulesDialog> m_ui;
    QScopedPointer<VmsRuleEditorWindow> m_ruleEditorWindow;

    nx::vms::rules::Engine::RuleSet m_ruleSet;
    std::unordered_set<QnUuid> m_modifiedRules;

    std::unique_ptr<entity_item_model::UniqueKeyListEntity<QnUuid>> m_rulesListEntity;
    std::unique_ptr<entity_item_model::EntityItemModel> m_rulesListModel;
    std::unique_ptr<VmsRulesFilterModel> m_rulesFilterModel;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
