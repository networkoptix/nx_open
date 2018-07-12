#include "camera_settings_license_watcher.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

#include <QtCore/QPointer>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/license_usage_helper.h>

#include <nx/client/desktop/common/utils/basic_text_provider.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

// ------------------------------------------------------------------------------------------------
// CameraSettingsLicenseWatcher::Private

class CameraSettingsLicenseWatcher::Private: public QObject
{
    CameraSettingsLicenseWatcher* const q = nullptr;

public:
    Private(CameraSettingsLicenseWatcher* q, CameraSettingsDialogStore* store):
        QObject(),
        q(q),
        m_textProvider(new BasicTextProvider()),
        m_store(store)
    {
        NX_ASSERT(m_store);
        connect(m_store, &CameraSettingsDialogStore::stateChanged, this, &Private::updateText);

        const auto watcher = new QnCamLicenseUsageWatcher(q->commonModule(), this);
        connect(watcher, &QnLicenseUsageWatcher::licenseUsageChanged, this, &Private::updateText);
    }

    AbstractTextProvider* licenseUsageTextProvider() const
    {
        return m_textProvider.data();
    }

    QnVirtualCameraResourceList cameras() const
    {
        return m_cameras;
    }

    void setCameras(const QnVirtualCameraResourceList& value)
    {
        if (m_cameras == value)
            return;

        m_cameras = value;
        updateText();
    }

private:
    void updateText()
    {
        m_textProvider->setText(calculateText());
    }

    QString calculateText() const
    {
        if (!m_store)
            return QString();

        QnCamLicenseUsageHelper helper(q->commonModule());
        if (m_store->state().recording.enabled.hasValue())
            helper.propose(m_cameras, m_store->state().recording.enabled());

        QStringList lines;
        for (auto type: helper.licenseTypes())
        {
            const int used = helper.usedLicenses(type);
            if (used == 0)
                continue;

            const int total = helper.totalLicenses(type);
            QString message = CameraSettingsLicenseWatcher::tr("%1 are used", "", used).arg(
                QnLicense::displayText(type, used, total));

            const int required = helper.requiredLicenses(type);
            if (required > 0)
            {
                message += setWarningStyleHtml(lit(" (%1)").arg(
                    CameraSettingsLicenseWatcher::tr("%n more required", "", required)));
            }

            lines << message;
        }

        return lines.join(lit("<br/>"));
    }

private:
    const QScopedPointer<BasicTextProvider> m_textProvider;
    QPointer<CameraSettingsDialogStore> m_store;
    QnVirtualCameraResourceList m_cameras;
};

// ------------------------------------------------------------------------------------------------
// CameraSettingsLicenseWatcher

CameraSettingsLicenseWatcher::CameraSettingsLicenseWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
    d(new Private(this, store))
{
}

CameraSettingsLicenseWatcher::~CameraSettingsLicenseWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnVirtualCameraResourceList CameraSettingsLicenseWatcher::cameras() const
{
    return d->cameras();
}

void CameraSettingsLicenseWatcher::setCameras(const QnVirtualCameraResourceList& value)
{
    d->setCameras(value);
}

AbstractTextProvider* CameraSettingsLicenseWatcher::licenseUsageTextProvider() const
{
    return d->licenseUsageTextProvider();
}

} // namespace desktop
} // namespace client
} // namespace nx
