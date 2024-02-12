// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop::ui::messages {

class UserGroups
{
    Q_DECLARE_TR_FUNCTIONS(Groups)

public:
    static bool removeGroups(QWidget* parent, const QSet<nx::Uuid>& groups, bool allowSilent = true);
};

} // nx::vms::client::desktop::ui::messages
