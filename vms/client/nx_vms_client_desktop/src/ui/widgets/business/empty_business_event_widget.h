// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef EMPTY_BUSINESS_EVENT_WIDGET_H
#define EMPTY_BUSINESS_EVENT_WIDGET_H

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class EmptyBusinessEventWidget;
}

class QnEmptyBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnEmptyBusinessEventWidget(QWidget *parent = 0);
    ~QnEmptyBusinessEventWidget();
private:
    QScopedPointer<Ui::EmptyBusinessEventWidget> ui;
};

#endif // EMPTY_BUSINESS_EVENT_WIDGET_H
