// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/common/widgets/alert_label.h>
#include <nx/vms/client/desktop/common/widgets/editable_label.h>
#include <nx/vms/client/desktop/common/widgets/slide_switch.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/field_validator.h>
#include <nx/vms/rules/rule.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include "helpers/rule_compatibility_manager.h"

class QLabel;

namespace nx::vms::client::desktop::rules {

class ActionParametersWidget;
class ActionTypePickerWidget;
class EditableTitleWidget;
class EventParametersWidget;
class EventTypePickerWidget;

class EditVmsRuleDialog:
    public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

public:
    explicit EditVmsRuleDialog(QWidget* parent = nullptr);

    void setRule(std::shared_ptr<vms::rules::Rule> rule, bool isNewRule);

    void accept() override;
    void reject() override;

protected:
    void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    QLabel* m_eventLabel{nullptr};
    QLabel* m_actionLabel{nullptr};
    EditableLabel* m_editableLabel{nullptr};
    QPushButton* m_deleteButton{nullptr};
    QWidget* m_contentWidget{nullptr};
    EventTypePickerWidget* m_eventTypePicker{nullptr};
    EventParametersWidget* m_eventEditorWidget{nullptr};
    ActionTypePickerWidget* m_actionTypePicker{nullptr};
    ActionParametersWidget* m_actionEditorWidget{nullptr};
    AlertLabel* m_alertLabel{nullptr};
    QPushButton* m_enabledButton{nullptr};
    QDialogButtonBox* m_buttonBox{nullptr};
    bool m_hasChanges{false};
    bool m_checkValidityOnChanges{false};

    std::shared_ptr<vms::rules::Rule> m_rule;
    std::unique_ptr<RuleCompatibilityManager> m_ruleCompatibilityManager;
    bool m_isNewRule{false};

    nx::utils::ScopedConnections m_scopedConnections;

    void displayComment();
    void displayRule();
    void displayState();
    void displayControls();

    void displayActionEditor();
    void displayEventEditor();

    void onCommentChanged(const QString& comment);
    void onDeleteClicked();
    void onScheduleClicked();
    void onActionTypeChanged(const QString& actionType);
    void onEventTypeChanged(const QString& eventType);
    void onEnabledButtonClicked(bool checked);

    void updateButtonBox();

    void onEventFilterModified();
    void onActionBuilderModified();

    void setHasChanges(bool hasChanges);

    void showWarningIfRequired();

    vms::rules::ValidationResult ruleValidity() const;
};

} // namespace nx::vms::client::desktop::rules
