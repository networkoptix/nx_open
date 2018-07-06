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

#include <set>

#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/resource/camera.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <rest/server/json_rest_handler.h>
#include <utils/common/connective.h>

class QnCachingCameraAdvancedParamsReader;

class QnCameraSettingsRestHandler: public Connective<QnJsonRestHandler>
{
    Q_OBJECT
	typedef Connective<QnJsonRestHandler> base_type;

public:
    QnCameraSettingsRestHandler();
    virtual ~QnCameraSettingsRestHandler() override;

    virtual int executeGet(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* /*owner*/) override;

private:
    QScopedPointer<QnCachingCameraAdvancedParamsReader> m_paramsReader;
};
