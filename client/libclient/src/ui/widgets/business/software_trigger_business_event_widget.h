#pragma once

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
class SoftwareTriggerBusinessEventWidget;
} // namespace Ui

//TODO: #vkutin This implementation is a stub
// Actual implementation will be done as soon as software triggers specification is ready.

class QnSoftwareTriggerBusinessEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnSoftwareTriggerBusinessEventWidget(QWidget* parent = nullptr);
    virtual ~QnSoftwareTriggerBusinessEventWidget();

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;
    void at_usersButton_clicked();

private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::SoftwareTriggerBusinessEventWidget> ui;
};
