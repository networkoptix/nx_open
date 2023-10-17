// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_state_manager.h"

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/utils/context_utils.h>
#include <nx/vms/client/desktop/window_context.h>

using namespace nx::vms::client::desktop;

QnSessionAwareDelegate::QnSessionAwareDelegate(QObject* parent):
    CurrentSystemContextAware(parent),
    SessionAwareDelegate(windowContext()->workbenchStateManager())
{
}

QnSessionAwareDelegate::QnSessionAwareDelegate(WindowContext* windowContext):
    CurrentSystemContextAware(windowContext),
    SessionAwareDelegate(windowContext->workbenchStateManager())
{
}
