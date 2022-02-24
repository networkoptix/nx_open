// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_set>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>

#include <nx/vms/rules/rule.h>

namespace Ui { class VmsRuleEditorWindow; }

namespace nx::vms::client::desktop {
namespace vms_rules {

class VmsRulesItemDelegate;

class VmsRuleEditorWindow: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    VmsRuleEditorWindow(QWidget* parent = nullptr);
    virtual ~VmsRuleEditorWindow() override;

    void setRule(nx::vms::rules::Rule* rule);

private:
    QScopedPointer<Ui::VmsRuleEditorWindow> m_ui;
    nx::vms::rules::Rule* m_displayedRule;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
