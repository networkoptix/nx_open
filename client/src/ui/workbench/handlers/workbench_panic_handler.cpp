
#if 0

#include "workbench_panic_handler.h"

#include <QtGui/QAction>

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>

QnWorkbenchPanicHandler::QnWorkbenchPanicHandler(QObject *parent):  
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::TogglePanicModeAction),                  SIGNAL(toggled(bool)),  this,   SLOT(at_togglePanicModeAction_toggled(bool)));
}

QnWorkbenchPanicHandler::~QnWorkbenchPanicHandler() {
    return;
}

#endif