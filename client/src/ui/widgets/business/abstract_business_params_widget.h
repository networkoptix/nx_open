#ifndef ABSTRACT_BUSINESS_PARAMS_WIDGET_H
#define ABSTRACT_BUSINESS_PARAMS_WIDGET_H

#include <QWidget>

#include <events/business_logic_common.h>

class QnAbstractBusinessParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnAbstractBusinessParamsWidget(QWidget *parent = 0):QWidget(parent) {}
    ~QnAbstractBusinessParamsWidget() {}

    virtual void loadParameters(const QnBusinessParams &params) = 0;
    virtual QnBusinessParams parameters() = 0;

};

#endif // ABSTRACT_BUSINESS_PARAMS_WIDGET_H
