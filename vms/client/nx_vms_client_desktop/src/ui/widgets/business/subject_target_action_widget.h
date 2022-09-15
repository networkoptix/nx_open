// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QScopedPointer>
#include <QtGui/QValidator>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/business/abstract_business_params_widget.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/utils/uuid.h>

class QPushButton;

namespace nx { namespace vms { namespace event {
class QnBusinessStringsHelper;
}}} // namespace nx::vms::event

class QnSubjectValidationPolicy;

using CustomizableOptions = nx::vms::client::desktop::ui::SubjectSelectionDialog::CustomizableOptions;

class QnSubjectTargetActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnSubjectTargetActionWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent = nullptr);
    virtual ~QnSubjectTargetActionWidget() override;

protected:
    virtual void at_model_dataChanged(Fields fields) override;

    // TODO: #vkutin #3.2 Refactor this along with all event/action widgets.
    void setSubjectsButton(QPushButton* button);

    void setValidationPolicy(QnSubjectValidationPolicy* policy); //< Takes ownership.

    void setDialogOptions(const CustomizableOptions& options);

    void updateSubjectsButton();

private:
    void selectSubjects();

    QValidator::State roleValidity(const QnUuid& roleId) const;
    bool userValidity(const QnUserResourcePtr& user) const;
    QString calculateAlert(bool allUsers, const QSet<QnUuid>& subjects) const;

private:
    QPushButton* m_subjectsButton = nullptr;
    QScopedPointer<nx::vms::event::StringsHelper> m_helper;
    QScopedPointer<QnSubjectValidationPolicy> m_validationPolicy;
    std::optional<CustomizableOptions> m_options;
};
