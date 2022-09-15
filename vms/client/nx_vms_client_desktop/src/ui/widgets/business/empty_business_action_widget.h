// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class EmptyBusinessActionWidget;
}

class QnEmptyBusinessActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnEmptyBusinessActionWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::event::ActionType actionType,
        QWidget* parent = nullptr);
    ~QnEmptyBusinessActionWidget();

private:
    QScopedPointer<Ui::EmptyBusinessActionWidget> ui;
};
