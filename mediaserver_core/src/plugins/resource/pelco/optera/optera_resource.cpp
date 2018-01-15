#include "optera_resource.h"

#ifdef ENABLE_ONVIF

#include <plugins/utils/multisensor_data_provider.h>
#include <plugins/resource/onvif/onvif_resource_information_fetcher.h>
#include <nx/network/http/asynchttpclient.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_properties.h>
#include <common/static_common_module.h>

namespace
{
    const QString kOnvifResourceTypeName("ONVIF");

    const QString kManufacture("PelcoOptera");
    const QString kOpteraDefaultLogin("admin");
    const QString kOpteraDefaultPassword("admin");

    const QString kOnvifDeviceService("/onvif/device_service");

    const QString kPanomersiveMode("Panomersive");
    const QString kTiledMode("Tiled");
    const QString kUnistreamMode("Unistream");

    const QString kGetStitchingModeQueryTpl(":/camera_advanced_params/optera_get_stitching_mode_query.xml");
    const QString kSetStitchingModeQueryTpl(":/camera_advanced_params/optera_set_stitching_mode_query.xml");

    const QString kStitchingModeTplParam("{StitchingMode}");

    const int kMinimumTiledModeFirmwareVersion = 26;

    const uint kChangeCameraModeTimeout = 4000;

    const int kSoapSendTimeout = 30;
    const int kSoapRecieveTimeout = 30;
}

QnOpteraResource::QnOpteraResource() :
    m_videoLayout(nullptr)
{
    qDebug() << "Creating Optera resource";
}

QnOpteraResource::~QnOpteraResource()
{

}

QnConstResourceVideoLayoutPtr QnOpteraResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    QN_UNUSED(dataProvider);

    if (m_videoLayout)
        return m_videoLayout;

    auto resData = qnStaticCommon->dataPool()->data(getVendor(), getModel());
    auto layoutStr = resData.value<QString>(Qn::VIDEO_LAYOUT_PARAM_NAME2);

    if (!layoutStr.isEmpty())
    {
        m_videoLayout = QnResourceVideoLayoutPtr(
            QnCustomResourceVideoLayout::fromString(layoutStr));

        qDebug() << "Optera's video layout" << m_videoLayout->toString();
    }
    else
    {
        m_videoLayout = QnResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout());
    }

    auto resourceId = getId();

    commonModule()->propertyDictionary()->setValue(resourceId, Qn::VIDEO_LAYOUT_PARAM_NAME, m_videoLayout->toString());
    commonModule()->propertyDictionary()->saveParams(resourceId);

    return m_videoLayout;
}

nx::mediaserver::resource::StreamCapabilityMap QnOpteraResource::getStreamCapabilityMapFromDrives(
    bool primaryStream)
{
    return base_type::getStreamCapabilityMapFromDrives(primaryStream);
}

CameraDiagnostics::Result QnOpteraResource::initializeCameraDriver()
{
    QString urlStr = getUrl();

    if (!urlStr.startsWith("http://"))
        urlStr = lit("http://") + urlStr;

    QUrl url = urlStr;
    auto auth = getAuth();
    auto firmware = getFirmware();

    auto firmwareVersion = firmware
        .replace('.', "")
        .left(2)
        .toInt();

    if (firmwareVersion < kMinimumTiledModeFirmwareVersion)
        return base_type::initializeCameraDriver();

    if (auth.isNull())
    {
        auth.setUser(kOpteraDefaultLogin);
        auth.setPassword(kOpteraDefaultPassword);
    }

    CLSimpleHTTPClient http(
        url.host(),
        url.port(nx_http::DEFAULT_HTTP_PORT),
        kChangeCameraModeTimeout,
        auth);

    QByteArray response;

    auto status = makeGetStitchingModeRequest(http, response);
    QString currentStitchingMode;

    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();

    currentStitchingMode = getCurrentStitchingMode(response);

    if (currentStitchingMode == kTiledMode)
        return base_type::initializeCameraDriver();

    status = makeSetStitchingModeRequest(http, kTiledMode);

    if (status != CL_HTTP_SUCCESS)
        qDebug() << "Set stitching mode request failed (it's normal)";

    setOnvifRequestsRecieveTimeout(kSoapRecieveTimeout);
    setOnvifRequestsSendTimeout(kSoapSendTimeout);

    return CameraDiagnostics::InitializationInProgress();
}

QnAbstractStreamDataProvider* QnOpteraResource::createLiveDataProvider()
{
    if (!isInitialized())
        return nullptr;

    return new nx::plugins::utils::MultisensorDataProvider(toSharedPointer());
}


CLHttpStatus QnOpteraResource::makeGetStitchingModeRequest(CLSimpleHTTPClient& http, QByteArray& response) const
{
    QFile tpl(kGetStitchingModeQueryTpl);

    if (!tpl.open(QIODevice::ReadOnly))
    {
        qWarning() << "Optera resource, can't open query file (get)";
        return CL_HTTP_BAD_REQUEST;
    }

    auto status = http.doPOST(kOnvifDeviceService, tpl.readAll());

    if (status == CL_HTTP_SUCCESS)
        http.readAll(response);

    return status;
}

CLHttpStatus QnOpteraResource::makeSetStitchingModeRequest(CLSimpleHTTPClient& http, const QString& mode) const
{
    QFile tpl(kSetStitchingModeQueryTpl);

    if (!tpl.open(QIODevice::ReadOnly))
    {
        qWarning() << "Optera resource, can't open query file (set)";
        return CL_HTTP_BAD_REQUEST;
    }

    auto query = QString::fromLatin1(tpl.readAll());
    query = query.replace(kStitchingModeTplParam, mode);

    auto status = http.doPOST(kOnvifDeviceService, query.toLatin1());

    return status;
}

QString QnOpteraResource::getCurrentStitchingMode(const QByteArray& response) const
{
    auto str = QString::fromUtf8(response);
    QRegExp stitchingModeRx("StitchingMode>([^<]*)</");

    stitchingModeRx.indexIn(str);
    auto matches = stitchingModeRx.capturedTexts();

    if (matches.size() != 2 )
        return QString();

    return matches[1];

}

#endif // ENABLE_ONVIF
