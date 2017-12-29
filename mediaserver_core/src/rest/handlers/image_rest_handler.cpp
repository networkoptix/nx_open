#include "image_rest_handler.h"

#include <QtCore/QBuffer>

extern "C" {
#include <libavutil/avutil.h> //< for AV_NOPTS_VALUE
} // extern "C"

#include <network/tcp_connection_priv.h>
#include <utils/math/math.h>
#include <core/resource/network_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <camera/get_image_helper.h>
#include <utils/common/util.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <api/helpers/camera_id_helper.h>
#include <common/common_module.h>

int QnImageRestHandler::noVideoError(QByteArray& result, qint64 time) const
{
    result.append("<root>\n");
    result.append("No video for time ");
    if (time == DATETIME_NOW)
        result.append("now");
    else
        result.append(QByteArray::number(time));
    result.append("</root>\n");
    return CODE_INVALID_PARAMETER;
}

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedResIdParam = lit("res_id");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");
static const QStringList kCameraIdParams{
    kCameraIdParam, kDeprecatedResIdParam, kDeprecatedPhysicalIdParam};

} // namespace

QStringList QnImageRestHandler::cameraIdUrlParams() const
{
    return kCameraIdParams;
}

int QnImageRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    NX_LOG(lit("QnImageRestHandler: received request %1").arg(path), cl_logDEBUG1);

    QString errStr;
    qint64 time = AV_NOPTS_VALUE;
    QnThumbnailRequestData::RoundMethod roundMethod = QnThumbnailRequestData::KeyFrameBeforeMethod;
    QByteArray format("jpg");
    QSize dstSize;
    int rotate = -1;

    QString notFoundCameraId = QString::null;
    QnSecurityCamResourcePtr camera = nx::camera_id_helper::findCameraByFlexibleIds(
        owner->resourcePool(), &notFoundCameraId, params.toHash(), kCameraIdParams);
    if (!camera)
    {
        result.append("<root>\n");
        if (notFoundCameraId.isNull())
            result.append(lit("Missing 'cameraId' parameter"));
        else
            result.append(lit("Camera resource %1 not found").arg(notFoundCameraId));
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    // Validating input parameters.
    auto widthIt = params.find("width");
    auto heightIt = params.find("height");
    // Width cannot be specified without height, but height-only is acceptable.
    if (widthIt != params.end() && (heightIt == params.end() || heightIt->second.toInt() < 1))
        return nx::network::http::StatusCode::badRequest;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "time")
        {
            if (params[i].second.toLower().trimmed() == "latest")
                time = QnThumbnailRequestData::kLatestThumbnail;
            else
                time = nx::utils::parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "rotate")
        {
            rotate = params[i].second.toInt();
        }
        else if (params[i].first == "method")
        {
            QString val = params[i].second.toLower().trimmed();
            if (val == lit("before"))
            {
                roundMethod = QnThumbnailRequestData::KeyFrameBeforeMethod;
            }
            else if (val == lit("precise") || val == lit("exact"))
            {
                #if defined(EDGE_SERVER)
                    roundMethod = QnThumbnailRequestData::KeyFrameBeforeMethod;
                    qWarning() << "Get image performance hint: "
                        << "Ignore precise round method to reduce CPU usage";
                #else
                    roundMethod = QnThumbnailRequestData::PreciseMethod;
                #endif
            }
            else if (val == lit("after"))
            {
                roundMethod = QnThumbnailRequestData::KeyFrameAfterMethod;
            }
        }
        else if (params[i].first == "format")
        {
            format = params[i].second.toUtf8();
            if (format == "jpg")
                format = "jpeg";
            else if (format == "tif")
                format = "tiff";
        }
        else if (params[i].first == "height")
        {
            dstSize.setHeight(params[i].second.toInt());
        }
        else if (params[i].first == "width")
        {
            dstSize.setWidth(params[i].second.toInt());
        }
    }
    if (time == (qint64) AV_NOPTS_VALUE)
        errStr = QLatin1String("parameter 'time' is absent");
    else if (dstSize.height() >= 0 && dstSize.height() < 8)
        errStr = QLatin1String("Parameter height must be >= 8");
    else if (dstSize.width() >= 0 && dstSize.width() < 8)
        errStr = QLatin1String("Parameter width must be >= 8");
    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    auto requiredPermission =
        (time == 0 || time == (qint64) AV_NOPTS_VALUE)
            ? Qn::Permission::ViewLivePermission
            : Qn::Permission::ViewFootagePermission;

    if (!owner->commonModule()->resourceAccessManager()->hasPermission(
        owner->accessRights(), camera, requiredPermission))
    {
        result.append("<root>\n");
        result.append(lit("Access denied: Insufficent access rights"));
        result.append("</root>\n");
        return nx::network::http::StatusCode::forbidden;
    }

    CLVideoDecoderOutputPtr outFrame =
        QnGetImageHelper::getImage(camera, time, dstSize, roundMethod, rotate);
    if (!outFrame)
        return noVideoError(result, time);

    if (format == "jpg" || format == "jpeg")
    {
        QByteArray encodedData = QnGetImageHelper::encodeImage(outFrame, format);
        result.append(encodedData);
    }
    else
    {
        // Prepare image using Qt.
        QImage image = outFrame->toImage();
        QBuffer output(&result);
        image.save(&output, format);
    }

    if (result.isEmpty())
    {
        result.append("<root>\n");
        result.append(QString("Invalid image format '%1'").arg(QString(format)));
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    contentType = QByteArray("image/") + format;
    return CODE_OK;
}

int QnImageRestHandler::executePost(
    const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result,
    QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
