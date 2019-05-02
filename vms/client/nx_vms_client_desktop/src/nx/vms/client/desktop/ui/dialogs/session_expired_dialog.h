#pragma once

#include <QtCore/QCoreApplication>

class QnWorkbenchContext;

namespace nx::vms::client::desktop
{

/** Shows "Your session has expired" modal dialog. */

class SessionExpiredDialog
{
    Q_DECLARE_TR_FUNCTIONS(SessionExpiredDialog)

public:
    static void exec(QnWorkbenchContext* context);
};

}