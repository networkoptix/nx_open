#ifndef ABSTRACT_BUSINESS_EVENT_WIDGET_H
#define ABSTRACT_BUSINESS_EVENT_WIDGET_H

#include <QWidget>

#include <events/business_logic_common.h>

class QnAbstractBusinessEventWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnAbstractBusinessEventWidget(QWidget *parent = 0):QWidget(parent) {}
    ~QnAbstractBusinessEventWidget() {}

    virtual void loadParameters(const QnBusinessParams &params) = 0;
    virtual QnBusinessParams parameters() = 0;

};

#endif // ABSTRACT_BUSINESS_EVENT_WIDGET_H
