#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchAlarmLayoutHandler: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchAlarmLayoutHandler(QObject *parent = nullptr);
    virtual ~QnWorkbenchAlarmLayoutHandler();

private:
    void openCamerasInAlarmLayout(const QnVirtualCameraResourceList &cameras);

};
