#pragma once

#include <ui/delegates/select_resources_delegate_editor_button.h>

#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>

class QnResourceSelectionDialogDelegate;
class QnSubjectValidationPolicy;

using CustomizableOptions = nx::vms::client::desktop::ui::SubjectSelectionDialog::CustomizableOptions;

class QnSelectUsersDialogButton : public QnSelectResourcesDialogButton
{
    Q_OBJECT
    using base_type = QnSelectResourcesDialogButton;

public:
    explicit QnSelectUsersDialogButton(QWidget* parent = nullptr);
    virtual ~QnSelectUsersDialogButton() override;

    QnResourceSelectionDialogDelegate* getDialogDelegate() const;
    void setDialogDelegate(QnResourceSelectionDialogDelegate* delegate);

    void setSubjectValidationPolicy(QnSubjectValidationPolicy* policy); //< Takes ownership.
    void setDialogOptions(const CustomizableOptions& options);

protected:
    virtual void handleButtonClicked() override;

private:
    QnResourceSelectionDialogDelegate* m_dialogDelegate;
    QScopedPointer<QnSubjectValidationPolicy> m_subjectValidation;
    std::optional<CustomizableOptions> m_options;
};
