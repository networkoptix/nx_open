// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>

Q_MOC_INCLUDE("core/resource/camera_resource.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/event_search/utils/text_filter_setup.h")

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class AbstractSearchListModel;
class TextFilterSetup;

class CommonObjectSearchSetup: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::TextFilterSetup* textFilter READ textFilter
        NOTIFY modelChanged)

    Q_PROPERTY(nx::vms::client::desktop::RightPanel::TimeSelection timeSelection
        READ timeSelection WRITE setTimeSelection NOTIFY timeSelectionChanged)

    Q_PROPERTY(nx::vms::client::desktop::RightPanel::CameraSelection cameraSelection
        READ cameraSelection WRITE setCameraSelection NOTIFY cameraSelectionChanged)

    Q_PROPERTY(QnVirtualCameraResourceSet selectedCameras READ selectedCameras
            NOTIFY selectedCamerasChanged)

    Q_PROPERTY(QnVirtualCameraResource* singleCamera READ singleCameraRaw
        NOTIFY selectedCamerasChanged)

    Q_PROPERTY(int cameraCount READ cameraCount NOTIFY selectedCamerasChanged)

    /** Whether the system has both cameras and I/O modules. */
    Q_PROPERTY(bool mixedDevices READ mixedDevices NOTIFY mixedDevicesChanged)

public:
    explicit CommonObjectSearchSetup(QObject* parent = nullptr);
    virtual ~CommonObjectSearchSetup() override;

    AbstractSearchListModel* model() const;
    void setModel(AbstractSearchListModel* value);

    QnWorkbenchContext* context() const;
    void setContext(QnWorkbenchContext* value);

    TextFilterSetup* textFilter() const;

    RightPanel::TimeSelection timeSelection() const;
    void setTimeSelection(RightPanel::TimeSelection value);

    RightPanel::CameraSelection cameraSelection() const;
    void setCameraSelection(RightPanel::CameraSelection value);
    void setCustomCameras(const QnVirtualCameraResourceSet& value);
    Q_INVOKABLE bool chooseCustomCameras();

    QnVirtualCameraResourceSet selectedCameras() const;
    QnVirtualCameraResourcePtr singleCamera() const;
    int cameraCount() const;
    bool mixedDevices() const;

private:
    QnVirtualCameraResource* singleCameraRaw() const;

signals:
    void modelChanged();
    void contextChanged();
    void timeSelectionChanged();
    void cameraSelectionChanged();

    void selectedCamerasChanged();
    void singleCameraChanged();
    void mixedDevicesChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
