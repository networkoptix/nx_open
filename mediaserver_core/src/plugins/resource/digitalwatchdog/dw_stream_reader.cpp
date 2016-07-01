#include "dw_stream_reader.h"
#include <utils/network/simple_http_client.h>
#include <utils/common/log.h>

namespace
{
    const uint kDWHttpTimeout = 3000;
    const QString kCameraControlUrl = lit("/cgi-bin/camerasetup.cgi");
    const QString kFirstStreamProfileParamName = lit("codec_ext1");
    const QString kSecondStreamProfileParamName = lit("codec_ext2");
    const QString kHighH264ProfileParamValue = lit("h264_high");
    const QString kBaselineH264ProfileParamName = lit("h264_baseline");
}

QnDWStreamReader::QnDWStreamReader(const QnResourcePtr &res) :
    QnOnvifStreamReader(res)
{
}

void QnDWStreamReader::postStreamConfigureHook()
{
    auto secRes = m_resource.dynamicCast<QnSecurityCamResource>();

    if (!secRes)
        return;

    auto url = secRes->getUrl();
    auto auth = secRes->getAuth();

    CLSimpleHTTPClient http(url, kDWHttpTimeout, auth);

    QString body = lit("%1=%2&%3=%4")
        .arg(kFirstStreamProfileParamName)
        .arg(kHighH264ProfileParamValue)
        .arg(kSecondStreamProfileParamName)
        .arg(kHighH264ProfileParamValue);

    CLHttpStatus result = http.doPOST(kCameraControlUrl, body);

    if (result != CL_HTTP_SUCCESS)
    {
        auto message = lit(
            "QnDWStreamReader: couldn't set stream profile to `High` "
            "for camera %1 (%2)")
            .arg(secRes->getModel())
            .arg(secRes->getUrl());

        NX_LOG(message, cl_logDEBUG1);
        qDebug() << message;
    }
}
