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

#include "rest/server/request_handler.h"


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

          ok: param1
          failure: param2
          ok: param3

    \a res_id - unique id of camera
    If failed to set just one param, response status code is set to 500 (Internal Server Error), in other case - 200 (OK)
*/

struct AwaitedParameters;

class QnCameraSettingsRestHandler
:
    public QnRestRequestHandler
{
    Q_OBJECT

public:
    //!Implementation of QnRestRequestHandler::executeGet
    virtual int executeGet( const QString& path, const QnRequestParamList& params, QByteArray& responseMessageBody, QByteArray& contentType);
    //!Implementation of QnRestRequestHandler::executePost
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& requestBody, const QByteArray& srcBodyContentType, QByteArray& responseMessageBody, QByteArray& contentType);
    //!Implementation of QnRestRequestHandler::description
    //virtual QString description(TCPSocket* tcpSocket) const;

private:
    QMutex m_mutex;
    QWaitCondition m_cond;
    std::set<AwaitedParameters*> m_awaitedParamsSets;

private slots:
    void asyncParamGetComplete(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result);
    void asyncParamSetComplete(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result);
};

//!Handles getCameraParam request
/*!
    Derived only to allow correct description. All request processing is done in \a QnCameraSettingsHandler class
*/
class QnSetCameraParamRestHandler
:
    public QnCameraSettingsRestHandler
{
    Q_OBJECT
public:
};

//!Handles setCameraParam request
/*!
    Derived only to allow correct description. All request processing is done in \a QnCameraSettingsHandler class
*/
class QnGetCameraParamRestHandler
:
    public QnCameraSettingsRestHandler
{
    Q_OBJECT
public:
    //!Implementation of QnRestRequestHandler::description
};

#endif  //QN_CAMERA_SETTINGS_HANDLER_H
