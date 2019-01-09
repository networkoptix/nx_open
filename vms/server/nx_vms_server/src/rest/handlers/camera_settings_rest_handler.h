#pragma once

/**@file
* Handles requests to set/get camera parameters. All methods are re-enterable.
*
* Examples:
* c->s: GET /api/getCameraParam?res_id=camera_mac&param1&param2&param3 HTTP/1.0
* s->c: HTTP/1.0 200 OK
*       Content-Type: application/text
*       Content-Length: 39
*
*       param1=value1
*       param2=value2
*       param3=value3
*
* To set:
* c->s: GET /api/setCameraParam?res_id=camera_mac&param1=value1&param2=value2&param3=value3 HTTP/1.0
* s->c: HTTP/1.0 500 Internal Server Error
*       Content-Type: application/text
*       Content-Length: 38
*
*       param1=value1
*       param3=value3
*
* res_id - unique id of camera
* If failed to set just one param, response status code is set to 500 (Internal Server Error), in
* other case - 200 (OK).
*/

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/camera.h>
#include <rest/server/json_rest_handler.h>
#include <utils/common/connective.h>

class QnCachingCameraAdvancedParamsReader;

struct QnCameraSettingsRestHandlerPostBody; //< Private - made public for Fusion.

class QnCameraSettingsRestHandler: public Connective<QnJsonRestHandler>
{
    Q_OBJECT
    typedef Connective<QnJsonRestHandler> base_type;

public:
    QnCameraSettingsRestHandler(QnResourceCommandProcessor* commandProcessor);
    virtual ~QnCameraSettingsRestHandler() override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    using StatusCode = nx::network::http::StatusCode::Value;
    using PostBody = QnCameraSettingsRestHandlerPostBody;

    StatusCode obtainCameraFromRequestParams(
        const QnRequestParams& requestParams,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner,
        nx::vms::server::resource::CameraPtr* outCamera) const;

    StatusCode obtainCameraFromPostBody(
        const PostBody& postBody,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner,
        nx::vms::server::resource::CameraPtr* outCamera) const;

    StatusCode obtainCameraParamValuesFromRequestParams(
        const nx::vms::server::resource::CameraPtr& camera,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        QnCameraAdvancedParamValueMap* outValues) const;

    StatusCode obtainCameraParamValuesFromPostBody(
        const nx::vms::server::resource::CameraPtr& camera,
        const PostBody& body,
        QnJsonRestResult& result,
        QnCameraAdvancedParamValueMap* outValues) const;

    StatusCode handleGetParamsRequest(
        const QnRestConnectionProcessor* owner,
        const QnVirtualCameraResourcePtr& camera,
        const QSet<QString>& requestedParameterIds,
        QnCameraAdvancedParamValueMap* outParameterMap);

    StatusCode handleSetParamsRequest(
        const QnRestConnectionProcessor* owner,
        const QnVirtualCameraResourcePtr& camera,
        const QnCameraAdvancedParamValueMap& parametersToSet,
        QnCameraAdvancedParamValueMap* outParameterMap);

private:
    QScopedPointer<QnCachingCameraAdvancedParamsReader> m_paramsReader;
    QnResourceCommandProcessor* m_commandProcessor = nullptr;
};
