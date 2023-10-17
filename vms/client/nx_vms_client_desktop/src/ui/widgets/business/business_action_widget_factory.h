// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/api/types/event_rule_types.h>

class QnAbstractBusinessParamsWidget;
namespace nx::vms::client::desktop { class WindowContext; }

class QnBusinessActionWidgetFactory {
public:
    static QnAbstractBusinessParamsWidget* createWidget(
        nx::vms::api::ActionType actionType,
        nx::vms::client::desktop::WindowContext* context,
        QWidget* parent = nullptr);
};
