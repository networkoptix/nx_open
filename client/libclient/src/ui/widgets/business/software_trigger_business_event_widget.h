#pragma once

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
class SoftwareTriggerBusinessEventWidget;
} // namespace Ui

class QnSoftwareTriggerBusinessEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnSoftwareTriggerBusinessEventWidget(QWidget* parent = nullptr);
    virtual ~QnSoftwareTriggerBusinessEventWidget();

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private:
    void at_usersButton_clicked();
    void paramsChanged();

private:
    QScopedPointer<Ui::SoftwareTriggerBusinessEventWidget> ui;
};
