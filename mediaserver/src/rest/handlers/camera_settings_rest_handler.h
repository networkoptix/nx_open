/**********************************************************
* 01 aug 2012
* a.kolesnikov
***********************************************************/

#ifndef QN_CAMERA_SETTINGS_HANDLER_H
#define QN_CAMERA_SETTINGS_HANDLER_H

#include <map>
#include <set>

#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <core/resource/resource_fwd.h>

#include <rest/server/json_rest_handler.h>
#include <utils/common/connective.h>


//!Handles requests to set/get camera parameters. All methods are re-enterable
/*!
    Examples:
    c->s: GET /api/getCameraParam?res_id=camera_mac&param1&param2&param3 HTTP/1.0
    s->c: HTTP/1.0 200 OK
          Content-Type: application/text
          Content-Length: 39

          param1=value1
          param2=value2
          param3=value3

    To set:
    c->s: GET /api/setCameraParam?res_id=camera_mac&param1=value1&param2=value2&param3=value3 HTTP/1.0
    s->c: HTTP/1.0 500 Internal Server Error
          Content-Type: application/text
          Content-Length: 38

          param1=value1
          param3=value3

    \a res_id - unique id of camera
    If failed to set just one param, response status code is set to 500 (Internal Server Error), in other case - 200 (OK)
*/

struct AwaitedParameters;
class QnCameraAdvancedParamsReader;

class QnCameraSettingsRestHandler: public Connective<QnJsonRestHandler> {
    Q_OBJECT

	typedef Connective<QnJsonRestHandler> base_type;
public:
	QnCameraSettingsRestHandler();
	virtual ~QnCameraSettingsRestHandler();

    //!Implementation of QnJsonRestHandler::executeGet
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    enum class Operation {
        GetParam,
        GetParamsBatch,
        SetParam,
        SetParamsBatch
    };

    void connectToResource(const QnResourcePtr &resource, Operation operation);
    void disconnectFromResource(const QnResourcePtr &resource, Operation operation);
    void processOperation(const QnResourcePtr &resource, Operation operation, const QnCameraAdvancedParamValueList &values);

private slots:
    void asyncParamGetComplete(const QnResourcePtr &resource, const QString &id, const QString &value, bool success);
    void asyncParamSetComplete(const QnResourcePtr &resource, const QString &id, const QString &value, bool success);
    void asyncParamsGetComplete(const QnResourcePtr &resource, const QnCameraAdvancedParamValueList &values);
    void asyncParamsSetComplete(const QnResourcePtr &resource, const QnCameraAdvancedParamValueList &values);

private:
    QMutex m_mutex;
    QWaitCondition m_cond;
    std::set<AwaitedParameters*> m_awaitedParamsSets;
    QScopedPointer<QnCameraAdvancedParamsReader> m_paramsReader;
};

#endif  //QN_CAMERA_SETTINGS_HANDLER_H
