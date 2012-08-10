#include "workbench_action_handler.h"

QnWorkbenchPanicHandler::QnWorkbenchPanicHandler(QObject *parent):  
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    foreach(const QnVideoServerResourcePtr &resource, resourcePool()->getResources())
}

QnWorkbenchPanicHandler::~QnWorkbenchPanicHandler() {

}

