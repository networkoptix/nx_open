#include "camera_settings_property_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/param.h>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_store.h>

namespace nx::vms::client::desktop {

class CameraSettingsPropertyWatcher::Private: public QObject
{
public:
    Private(CameraSettingsDialogStore* store): QObject(), store(store) {}
    virtual ~Private() override = default;

    void handlePropertyChanged(const QnResourcePtr& resource, const QString& key) const
    {
        if (!store)
            return;

        if (key == ResourcePropertyKey::kStreamUrls)
        {
            if (!store->state().isSingleCamera())
                return;

            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!NX_ASSERT(camera && cameras.contains(camera)))
                return;

            store->setStreamUrls(
                camera->sourceUrl(Qn::CR_LiveVideo),
                camera->sourceUrl(Qn::CR_SecondaryLiveVideo),
                ModificationSource::remote);
        }
    }

public:
    QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourceSet cameras;
    nx::utils::ScopedConnections connections;
};

CameraSettingsPropertyWatcher::CameraSettingsPropertyWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(store))
{
}

CameraSettingsPropertyWatcher::~CameraSettingsPropertyWatcher()
{
}

QnVirtualCameraResourceSet CameraSettingsPropertyWatcher::cameras() const
{
    return d->cameras;
}

void CameraSettingsPropertyWatcher::setCameras(const QnVirtualCameraResourceSet& value)
{
    if (d->cameras == value)
        return;

    d->connections.reset();
    d->cameras = value;

    for (const auto& camera: d->cameras)
    {
        d->connections << connect(camera.get(), &QnResource::propertyChanged,
            d.get(), &Private::handlePropertyChanged);
    }
}

} // namespace nx::vms::client::desktop