#include "camera_settings_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource_display_info.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsModel::Private
{
    QnVirtualCameraResourceList cameras;

    QnVirtualCameraResourcePtr singleCamera;
    CameraPersistentInfo persistentInfo;
    CameraNetworkInfo networkInfo;

    void updatePersistentInfo()
    {
        if (!singleCamera)
        {
            persistentInfo = CameraPersistentInfo();
            return;
        }

        persistentInfo.id = singleCamera->getId().toSimpleString();
        persistentInfo.firmware = singleCamera->getFirmware();
        persistentInfo.macAddress = singleCamera->getMAC().toString();
        persistentInfo.model = singleCamera->getModel();
        persistentInfo.vendor = singleCamera->getVendor();
    }

    void updateNetworkInfo()
    {
        if (!singleCamera)
        {
            networkInfo = CameraNetworkInfo();
            return;
        }

        networkInfo.ipAddress = QnResourceDisplayInfo(singleCamera).host();
        networkInfo.webPage = calculateWebPage();


        bool isIoModule = singleCamera->isIOModule();
        bool hasPrimaryStream = !isIoModule || singleCamera->isAudioSupported();
        networkInfo.primaryStream = hasPrimaryStream
            ? singleCamera->sourceUrl(Qn::CR_LiveVideo)
            : CameraSettingsModel::tr("I/O module has no audio stream");

        bool hasSecondaryStream = singleCamera->hasDualStreaming2();
        networkInfo.secondaryStream = hasSecondaryStream
            ? singleCamera->sourceUrl(Qn::CR_SecondaryLiveVideo)
            : CameraSettingsModel::tr("Camera has no secondary stream");
    }

    QString calculateWebPage() const
    {
        NX_EXPECT(singleCamera);
        QString webPageAddress = lit("http://") + singleCamera->getHostAddress();

        QUrl url = QUrl::fromUserInput(singleCamera->getUrl());
        if (url.isValid())
        {
            const QUrlQuery query(url);
            int port = query.queryItemValue(lit("http_port")).toInt();
            if (port == 0)
                port = url.port(80);

            if (port != 80 && port > 0)
                webPageAddress += QLatin1Char(':') + QString::number(url.port());
        }

        return lit("<a href=\"%1\">%1</a>").arg(webPageAddress);
    }
};

CameraSettingsModel::CameraSettingsModel(QnWorkbenchContext* context, QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    d(new Private())
{

}

CameraSettingsModel::~CameraSettingsModel()
{
}

QnVirtualCameraResourceList CameraSettingsModel::cameras() const
{
    return d->cameras;
}

void CameraSettingsModel::setCameras(const QnVirtualCameraResourceList& cameras)
{
    if (d->singleCamera)
        d->singleCamera->disconnect(this);

    d->cameras = cameras;
    if (d->cameras.size() == 1)
        d->singleCamera = d->cameras.first();
    else
        d->singleCamera.reset();

    if (d->singleCamera)
    {
        connect(d->singleCamera, &QnResource::urlChanged, this,
            [this]
            {
                d->updateNetworkInfo();
                emit networkInfoChanged();
            });
    }

    d->updatePersistentInfo();
    d->updateNetworkInfo();
}

bool CameraSettingsModel::isSingleCameraMode() const
{
    return d->singleCamera;
}

QString CameraSettingsModel::name() const
{
    if (d->singleCamera)
        return d->singleCamera->getName();

    return QnDeviceDependentStrings::getNumericName(resourcePool(), d->cameras);
}

void CameraSettingsModel::setName(const QString& name)
{
    NX_ASSERT(d->singleCamera);
    if (!d->singleCamera)
        return;

    d->singleCamera->setName(name);
}

CameraSettingsModel::CameraPersistentInfo CameraSettingsModel::info() const
{
    return d->persistentInfo;
}

CameraSettingsModel::CameraNetworkInfo CameraSettingsModel::networkInfo() const
{
    return d->networkInfo;
}

void CameraSettingsModel::pingCamera()
{
    menu()->trigger(ui::action::PingAction, {Qn::TextRole, d->networkInfo.ipAddress});
}

void CameraSettingsModel::showCamerasOnLayout()
{
    menu()->trigger(ui::action::OpenInNewTabAction, d->cameras);
}

void CameraSettingsModel::openEventLog()
{
    menu()->trigger(ui::action::CameraIssuesAction, d->cameras);
}

void CameraSettingsModel::openCameraRules()
{
    menu()->trigger(ui::action::CameraBusinessRulesAction, d->cameras);
}

} // namespace desktop
} // namespace client
} // namespace nx
