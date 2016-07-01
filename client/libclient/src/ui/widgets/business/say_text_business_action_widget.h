#ifndef SAY_TEXT_BUSINESS_ACTION_WIDGET_H
#define SAY_TEXT_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class SayTextBusinessActionWidget;
}

class QnSayTextBusinessActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnSayTextBusinessActionWidget(QWidget *parent = 0);
    ~QnSayTextBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();
    void enableTestButton();
    void at_testButton_clicked();
    void at_volumeSlider_valueChanged(int value);
private:
    QScopedPointer<Ui::SayTextBusinessActionWidget> ui;
};

#endif // SAY_TEXT_BUSINESS_ACTION_WIDGET_H
