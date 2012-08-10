#include "workbench_panic_handler.h"

QnWorkbenchPanicHandler::QnWorkbenchPanicHandler(QObject *parent):  
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    //foreach(const QnVideoServerResourcePtr &resource, resourcePool()->getResources())
}

QnWorkbenchPanicHandler::~QnWorkbenchPanicHandler() {

}

