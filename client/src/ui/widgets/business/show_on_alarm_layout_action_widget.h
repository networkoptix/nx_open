#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class ShowOnAlarmLayoutActionWidget;
}

class QnShowOnAlarmLayoutActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnShowOnAlarmLayoutActionWidget(QWidget *parent = 0);
    ~QnShowOnAlarmLayoutActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private:
    void selectUsers();
    void updateUsersButtonText();
    void paramsChanged();

private:
    QScopedPointer<Ui::ShowOnAlarmLayoutActionWidget> ui;
};
