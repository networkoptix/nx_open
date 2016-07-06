
#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

class QnAvailableCamerasWatcher;

class QnCurrentUserAvailableCamerasWatcher : public Connective<QObject>
    , QnWorkbenchContextAware
{
    Q_OBJECT

    Q_PROPERTY(QnVirtualCameraResourceList availableCameras READ availableCameras NOTIFY availableCamerasChanged)

    typedef Connective<QObject> base_type;

public:
    QnCurrentUserAvailableCamerasWatcher(QObject *parent);

    virtual ~QnCurrentUserAvailableCamerasWatcher();

    QnVirtualCameraResourceList availableCameras() const;

signals:
    void availableCamerasChanged();

    void userChanged();

private:
    QnAvailableCamerasWatcher * const m_watcher;
};