#include "optera_resource.h"
#include <utils/network/http/httpclient.h>

namespace 
{
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
}

QnOpteraResource::QnOpteraResource()
{

}

QnOpteraResource::~QnOpteraResource()
{

}

CameraDiagnostics::Result QnOpteraResource::initInternal()
{
    QUrl url = getUrl();
    auto auth = getAuth();
    auto firmware = getFirmware();

    auto firmwareVersion = firmware
        .replace('.', "")
        .left(2)
        .toInt();

    if (firmwareVersion < kMinimumTiledModeFirmwareVersion)
        return base_type::initInternal();

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
        return base_type::initInternal();

    setFlags(flags() | Qn::need_reinit_channels);

    status = makeSetStitchingModeRequest(http, kTiledMode);

    if (status != CL_HTTP_SUCCESS)
        qDebug() << "Set stitching mode request failed";

    return CameraDiagnostics::InitializationInProgress();
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
