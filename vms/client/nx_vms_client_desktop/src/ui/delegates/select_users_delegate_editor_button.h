#pragma once

#include <ui/delegates/select_resources_delegate_editor_button.h>

class QnResourceSelectionDialogDelegate;
class QnSubjectValidationPolicy;

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

protected:
    virtual void handleButtonClicked() override;

private:
    QnResourceSelectionDialogDelegate* m_dialogDelegate;
    QScopedPointer<QnSubjectValidationPolicy> m_subjectValidation;
};
