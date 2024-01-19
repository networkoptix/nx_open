// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/editable_label.h>
#include <nx/vms/client/desktop/common/widgets/slide_switch.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/rule.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop::rules {

class ActionTypePickerWidget;
class EditableTitleWidget;
class EventTypePickerWidget;

class EditVmsRuleDialog:
    public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

public:
    EditVmsRuleDialog(QWidget* parent = nullptr);

    void setRule(std::shared_ptr<vms::rules::Rule> rule);

    void accept() override;
    void reject() override;

protected:
    void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    EditableLabel* m_editableLabel{nullptr};
    QPushButton* m_deleteButton{nullptr};
    QWidget* m_contentWidget{nullptr};
    EventTypePickerWidget* m_eventTypePicker{nullptr};
    QWidget* m_eventEditorWidget{nullptr};
    ActionTypePickerWidget* m_actionTypePicker{nullptr};
    QWidget* m_actionEditorWidget{nullptr};
    QPushButton* m_enabledButton{nullptr};

    std::shared_ptr<vms::rules::Rule> m_rule;

    void displayComment();
    void displayRule();
    void displayState();
    void displayControls();

    void displayActionEditor(const QString& actionType);
    void displayEventEditor(const QString& eventType);

    void onCommentChanged(const QString& comment);
    void onDeleteClicked();
    void onScheduleClicked();
    void onActionTypeChanged(const QString& actionType);
    void onEventTypeChanged(const QString& eventType);
    void onEnabledButtonClicked(bool checked);
};

} // namespace nx::vms::client::desktop::rules
