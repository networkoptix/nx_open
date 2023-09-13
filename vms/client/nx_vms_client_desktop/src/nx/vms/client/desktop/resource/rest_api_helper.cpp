// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rest_api_helper.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

class RestApiHelper::Private
{
public:
    Private() {};
    virtual ~Private() = default;

public:
    nx::vms::common::SessionTokenHelperPtr sessionTokenHelper;
};

RestApiHelper::RestApiHelper(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

RestApiHelper::~RestApiHelper()
{
}

void RestApiHelper::setParent(QWidget *parentWidget)
{
    d->sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        parentWidget,
        tr(""),
        tr("Password confirmation is required to apply changes"),
        tr("Confirm"),
        FreshSessionTokenHelper::ActionType::updateSettings);
}

nx::vms::common::SessionTokenHelperPtr RestApiHelper::getSessionTokenHelper()
{
    return d->sessionTokenHelper;
}

} // namespace nx::vms::client::desktop
