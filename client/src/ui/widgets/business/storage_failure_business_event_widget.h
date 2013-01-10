#ifndef STORAGE_FAILURE_BUSINESS_EVENT_WIDGET_H
#define STORAGE_FAILURE_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include "abstract_business_params_widget.h"

namespace Ui {
class QnStorageFailureBusinessEventWidget;
}

class QnStorageFailureBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    
public:
    explicit QnStorageFailureBusinessEventWidget(QWidget *parent = 0);
    ~QnStorageFailureBusinessEventWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;
private:
    Ui::QnStorageFailureBusinessEventWidget *ui;
};

#endif // STORAGE_FAILURE_BUSINESS_EVENT_WIDGET_H
