// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::rules {

class EventTestDialog: public QmlDialogWrapper, public CurrentSystemContextAware
{
    Q_OBJECT

public:
    explicit EventTestDialog(QWidget* parent);

    Q_INVOKABLE void onEventSelected(const QString& eventType);

    Q_INVOKABLE void testEvent(const QString& event);

    static constexpr auto kDebugActionName = "Event Test dialog";
    static void registerAction();
};

} // namespace nx::vms::client::desktop::rules
