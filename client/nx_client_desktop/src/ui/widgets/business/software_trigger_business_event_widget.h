#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
class SoftwareTriggerBusinessEventWidget;
} // namespace Ui

namespace nx { namespace vms { namespace event {
class StringsHelper;
}}} // namespace nx::vms::event

class QnSoftwareTriggerBusinessEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnSoftwareTriggerBusinessEventWidget(QWidget* parent = nullptr);
    virtual ~QnSoftwareTriggerBusinessEventWidget();

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    void at_usersButton_clicked();
    void updateUsersButton();
    void paramsChanged();

    bool isRoleValid(const QnUuid& id) const;
    bool isUserValid(const QnUserResourcePtr& user) const;
    QString calculateAlert(bool allUsers, const QSet<QnUuid>& checkedSubjects) const;

private:
    QScopedPointer<Ui::SoftwareTriggerBusinessEventWidget> ui;
    QScopedPointer<nx::vms::event::StringsHelper> m_helper;
};
