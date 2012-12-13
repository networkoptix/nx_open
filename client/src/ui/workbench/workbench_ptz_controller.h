#ifndef QN_WORKBENCH_PTZ_CONTROLLER_H
#define QN_WORKBENCH_PTZ_CONTROLLER_H

#include <QtCore/QObject>

#include "workbench_context_aware.h"

class QnWorkbenchPtzController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPtzController(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzController();


private:

};




#endif // QN_WORKBENCH_PTZ_CONTROLLER_H
