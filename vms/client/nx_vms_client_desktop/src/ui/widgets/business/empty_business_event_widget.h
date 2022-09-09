// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

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
    explicit QnEmptyBusinessEventWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent = nullptr);
    ~QnEmptyBusinessEventWidget();
private:
    QScopedPointer<Ui::EmptyBusinessEventWidget> ui;
};
