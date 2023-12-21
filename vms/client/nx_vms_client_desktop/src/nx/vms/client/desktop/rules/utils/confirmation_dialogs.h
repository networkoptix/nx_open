// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

namespace nx::vms::client::desktop::rules {

class ConfirmationDialogs
{
    Q_DECLARE_TR_FUNCTIONS(VmsRulesDialogHelper)

public:
    static bool confirmDelete(QWidget* parent, size_t count = 1);
    static bool confirmReset(QWidget* parent);
};

} // namespace nx::vms::client::desktop::rules
