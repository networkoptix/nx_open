// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_license_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/license_usage_provider.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/license/license_usage_watcher.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/workbench/workbench_context.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

class CameraSettingsLicenseUsageProvider: public LicenseUsageProvider
{
public:
    virtual QString text() const override { return m_text; }
    virtual bool limitExceeded() const override { return m_limitExceeded; }

    void setData(const QString& text, bool limitExceeded)
    {
        if (m_text == text && m_limitExceeded == limitExceeded)
            return;

        m_text = text;
        m_limitExceeded = limitExceeded;

        emit stateChanged();
        emit textChanged(m_text);
    }

private:
    QString m_text;
    bool m_limitExceeded = false;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// CameraSettingsLicenseWatcher::Private

class CameraSettingsLicenseWatcher::Private: public QObject
{
    CameraSettingsLicenseWatcher* const q = nullptr;

public:
    Private(CameraSettingsLicenseWatcher* q, CameraSettingsDialogStore* store):
        QObject(),
        q(q),
        m_usageProvider(new CameraSettingsLicenseUsageProvider()),
        m_store(store)
    {
        NX_ASSERT(m_store);
        connect(m_store, &CameraSettingsDialogStore::stateChanged, this, &Private::updateText);

        using namespace nx::vms::common;
        connect(q->systemContext()->deviceLicenseUsageWatcher(),
            &LicenseUsageWatcher::licenseUsageChanged, this, &Private::updateText);
    }

    LicenseUsageProvider* licenseUsageProvider() const
    {
        return m_usageProvider.data();
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
        const auto info = calculateInfo();
        m_usageProvider->setData(info.usageText, info.limitExceeded);
    }

    struct LicenseInfo
    {
        QString usageText;
        bool limitExceeded = false;
    };

    LicenseInfo calculateInfo() const
    {
        if (!m_store)
            return {};

        nx::vms::license::CamLicenseUsageHelper helper(q->systemContext());

        if (m_store->state().recording.enabled.hasValue())
            helper.propose(m_cameras, m_store->state().recording.enabled());

        QStringList usedLicenses;
        if (m_store->state().recording.enabled.hasUser())
        {
            for (auto type: helper.licenseTypes())
            {
                const int used = helper.usedLicenses(type);
                if (used == 0)
                    continue;

                const int total = helper.totalLicenses(type);
                const auto message = CameraSettingsLicenseWatcher::tr("%1 are used",
                    "Text like '5/10 Professional Licenses' will be substituted", used)
                        .arg(QnLicense::displayText(type, used, total));

                usedLicenses << message;
            }
        }

        helper.propose(m_cameras, true);

        std::set<Qn::LicenseType> cameraLicenseTypes;
        for (const auto& camera: m_cameras)
            cameraLicenseTypes.insert(camera->licenseType());

        QStringList requiredLicenses;
        for (auto type: cameraLicenseTypes)
        {
            const int required = helper.requiredLicenses(type);
            if (required == 0)
                continue;

            const auto message = setWarningStyleHtml(CameraSettingsLicenseWatcher::tr(
                    "%1 are required",
                    "Text like '5 Professional Licenses' will be substituted", required)
                        .arg(QnLicense::displayText(type, required)));

            requiredLicenses << message;
        }

        using namespace nx::vms::common;
        return requiredLicenses.empty()
            ? LicenseInfo{usedLicenses.join(html::kLineBreak), /*limitExceeded*/ false}
            : LicenseInfo{requiredLicenses.join(html::kLineBreak), /*limitExceeded*/ true};
    }

private:
    const QScopedPointer<CameraSettingsLicenseUsageProvider> m_usageProvider;
    QPointer<CameraSettingsDialogStore> m_store;
    QnVirtualCameraResourceList m_cameras;
};

//-------------------------------------------------------------------------------------------------
// CameraSettingsLicenseWatcher

CameraSettingsLicenseWatcher::CameraSettingsLicenseWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
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

LicenseUsageProvider* CameraSettingsLicenseWatcher::licenseUsageProvider() const
{
    return d->licenseUsageProvider();
}

} // namespace nx::vms::client::desktop
