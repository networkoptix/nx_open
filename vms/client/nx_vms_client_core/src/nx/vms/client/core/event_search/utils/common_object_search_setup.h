// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <recording/time_period.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>

class QnCommonModule;

namespace nx::vms::client::core {

class AbstractSearchListModel;
class SystemContext;

class NX_VMS_CLIENT_CORE_API CommonObjectSearchSetup: public QObject
{
    Q_OBJECT

    Q_PROPERTY(nx::vms::client::core::TextFilterSetup* textFilter READ textFilter
        NOTIFY modelChanged)

    Q_PROPERTY(nx::vms::client::core::EventSearch::TimeSelection timeSelection
        READ timeSelection WRITE setTimeSelection NOTIFY timeSelectionChanged)

    Q_PROPERTY(nx::vms::client::core::EventSearch::CameraSelection cameraSelection
        READ cameraSelection WRITE setCameraSelection NOTIFY cameraSelectionChanged)

    Q_PROPERTY(QnVirtualCameraResourceSet selectedCameras
        READ selectedCameras
        WRITE setSelectedCameras
        NOTIFY selectedCamerasChanged)

    Q_PROPERTY(QnUuidList selectedCamerasIds
        READ selectedCamerasIds
        WRITE setSelectedCamerasIds
        NOTIFY selectedCamerasChanged)

    Q_PROPERTY(QnVirtualCameraResource* singleCamera READ singleCameraRaw
        NOTIFY selectedCamerasChanged)

    Q_PROPERTY(int cameraCount READ cameraCount NOTIFY selectedCamerasChanged)

    /** Whether the system has both cameras and I/O modules. */
    Q_PROPERTY(bool mixedDevices READ mixedDevices NOTIFY mixedDevicesChanged)

    Q_PROPERTY(nx::vms::client::core::AbstractSearchListModel* model
        READ model
        WRITE setModel
        NOTIFY modelChanged)

public:
    static void registerQmlType();

    explicit CommonObjectSearchSetup(QObject* parent = nullptr);
    virtual ~CommonObjectSearchSetup() override;

    SystemContext* context() const;
    void setContext(SystemContext* value);

    AbstractSearchListModel* model() const;
    void setModel(AbstractSearchListModel* value);
    TextFilterSetup* textFilter() const;

    EventSearch::TimeSelection timeSelection() const;
    void setTimeSelection(EventSearch::TimeSelection value);

    EventSearch::CameraSelection cameraSelection() const;
    void setCameraSelection(EventSearch::CameraSelection value);

    Q_INVOKABLE bool chooseCustomCameras();

    QnVirtualCameraResourceSet selectedCameras() const;
    void setSelectedCameras(const QnVirtualCameraResourceSet& value);

    QnUuidList selectedCamerasIds();
    void setSelectedCamerasIds(const QnUuidList& values);

    QnVirtualCameraResourcePtr singleCamera() const;
    int cameraCount() const;
    bool mixedDevices() const;

    /** Fills selected cameras list using available methods. */
    virtual bool selectCameras(QnUuidSet& selectedCameras) = 0;

    /** Should return currently selected camera in the client. */
    virtual QnVirtualCameraResourcePtr currentResource() const = 0;

    /** Return all cameras on the current layout*/
    virtual QnVirtualCameraResourceSet currentLayoutCameras() const = 0;

    virtual void clearTimelineSelection() const = 0;

protected:
    void handleTimelineSelectionChanged(const QnTimePeriod& selection);
    void updateRelevantCamerasForMode(EventSearch::CameraSelection mode);

private:
    QnVirtualCameraResource* singleCameraRaw() const;

signals:
    void modelChanged();
    void timeSelectionChanged();
    void cameraSelectionChanged();

    void selectedCamerasTextChanged();
    void selectedCamerasChanged();
    void singleCameraChanged();
    void mixedDevicesChanged();

    void parametersChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
