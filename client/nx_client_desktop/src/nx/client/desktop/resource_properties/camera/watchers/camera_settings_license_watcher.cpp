#include "camera_settings_license_watcher.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

#include <QtCore/QPointer>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/license_usage_helper.h>

#include <nx/client/desktop/common/utils/abstract_text_provider.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

// ------------------------------------------------------------------------------------------------
// CameraSettingsLicenseWatcher::Private

class CameraSettingsLicenseWatcher::Private: public AbstractTextProvider
{
    CameraSettingsLicenseWatcher* const q = nullptr;

public:
    Private(CameraSettingsLicenseWatcher* q, CameraSettingsDialogStore* store):
        AbstractTextProvider(q),
        q(q),
        m_store(store)
    {
        NX_ASSERT(m_store);
        connect(m_store, &CameraSettingsDialogStore::stateChanged, this, &Private::updateText);

        const auto watcher = new QnCamLicenseUsageWatcher(q->commonModule(), this);
        connect(watcher, &QnLicenseUsageWatcher::licenseUsageChanged, this, &Private::updateText);
    }

    virtual QString text() const override
    {
        return m_licenseMessageText;
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
        const auto newText = calculateText();
        if (m_licenseMessageText == newText)
            return;

        m_licenseMessageText = newText;
        emit textChanged(m_licenseMessageText);
    }

    QString calculateText() const
    {
        if (!m_store)
            return QString();

        QnCamLicenseUsageHelper helper(q->commonModule());
        if (m_store->state().recording.enabled.hasValue())
            helper.propose(m_cameras, m_store->state().recording.enabled());

        const QString colorId = qnGlobals->errorTextColor().name();

        QStringList lines;
        for (auto type: helper.licenseTypes())
        {
            const int used = helper.usedLicenses(type);
            if (used == 0)
                continue;

            const int total = helper.totalLicenses(type);
            QString message = tr("%n/%1 %2 licenses are used", "", used)
                .arg(total).arg(QnLicense::displayName(type));

            const int required = helper.requiredLicenses(type);
            if (required > 0)
            {
                message += lit(" <font color=%1>(%2)</font>")
                    .arg(colorId, tr("%n more required", "", required));
            }

            lines << message;
        }

        return lines.join(lit("<br>"));
    }

private:
    QPointer<CameraSettingsDialogStore> m_store;
    QnVirtualCameraResourceList m_cameras;
    QString m_licenseMessageText;
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
    return d;
}

} // namespace desktop
} // namespace client
} // namespace nx
