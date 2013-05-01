#ifndef QN_MEDIA_SERVER_CONNECTION_H
#define QN_MEDIA_SERVER_CONNECTION_H

#include <cassert>

#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QVector3D>
#include <QtGui/QRegion>

#include <utils/common/request_param.h>
#include <utils/common/util.h>

#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/statistics_reply.h>

#include <core/resource/resource_fwd.h>

#include <recording/time_period_list.h>

#include "api_fwd.h"
#include "media_server_cameras_data.h"


class QnPtzSpaceMapper;
class QnEnumNameMapper;

typedef QList<QPair<QString, bool> > QnStringBoolPairList;
typedef QList<QPair<QString, QVariant> > QnStringVariantPairList;

Q_DECLARE_METATYPE(QnStringBoolPairList);
Q_DECLARE_METATYPE(QnStringVariantPairList);


class QnMediaServerReplyProcessor: public QObject {
    Q_OBJECT

    typedef QObject base_type;

public:
    QnMediaServerReplyProcessor(int object);
    virtual ~QnMediaServerReplyProcessor();

    int object() const { return m_object; }

    bool isFinished() const { return m_finished; }
    int status() const { return m_status; }
    int handle() const { return m_handle; }
    QVariant reply() const { return m_reply; }

public slots:
    void processReply(const QnHTTPRawResponse &response, int handle);

signals:
    void finished(int status, int handle);
    void finished(int status, const QVariant &reply, int handle);
    void finished(int status, const QnStorageStatusReply &reply, int handle);
    void finished(int status, const QnStorageSpaceReply &reply, int handle);
    void finished(int status, const QnTimePeriodList &reply, int handle);
    void finished(int status, const QnStatisticsReply &reply, int handle);
    void finished(int status, const QnPtzSpaceMapper &reply, int handle);
    void finished(int status, const QVector3D &reply, int handle);
    void finished(int status, const QnStringVariantPairList &reply, int handle);
    void finished(int status, const QnStringBoolPairList &reply, int handle);

private:
    template<class T>
    void emitFinished(int status, const T &reply, int handle) {
        m_finished = true;
        m_status = status;
        m_handle = handle;
        m_reply = QVariant::fromValue<T>(reply);

        emit finished(status, reply, handle);
        emit finished(status, m_reply, handle);
    }

    void emitFinished(int status, int handle) {
        m_finished = true;
        m_status = status;
        m_handle = handle;
        m_reply = QVariant();

        emit finished(status, handle);
        emit finished(status, m_reply, handle);
    }

private:
    int m_object;
    
    bool m_finished;
    int m_status;
    int m_handle;
    QVariant m_reply;
};

class QnMediaServerRequestResult: public QObject {
    Q_OBJECT
public:
    int status() const { return m_status; }
    int handle() const { return m_handle; }
    const QVariant &reply() const { return m_reply; }

signals:
    void replyProcessed();

public slots:
    void processReply(int status, const QVariant &reply, int handle) {
        m_status = status;
        m_reply = reply;
        m_handle = handle;

        emit replyProcessed();
    }

private:
    int m_status;
    int m_handle;
    QVariant m_reply;
};

namespace detail {
    class QnMediaServerManualCameraReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerManualCameraReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_searchReplyReceived(const QnHTTPRawResponse& response, int handle);
        void at_addReplyReceived(const QnHTTPRawResponse& response, int handle);

    signals:
        void finishedSearch(const QnCamerasFoundInfoList &);
        void searchError(int, const QString &);
        void finishedAdd(int);
    };

    class QnMediaServerGetTimeReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerGetTimeReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(const QnHTTPRawResponse& response, int handle);

    signals:
        void finished(int status, const QDateTime &dateTime, int utcOffset, int handle);
    };

} // namespace detail

class QN_EXPORT QnMediaServerConnection: public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    QnMediaServerConnection(const QUrl &mediaServerApiUrl, QObject *parent = NULL);
    virtual ~QnMediaServerConnection();

    void setProxyAddr(const QUrl& apiUrl, const QString &addr, int port);
    int getProxyPort() { return m_proxyPort; }
    QString getProxyHost() { return m_proxyAddr; }

    QnTimePeriodList recordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs = 0, qint64 endTimeMs = INT64_MAX, qint64 detail = 1, const QList<QRegion> &motionRegions = QList<QRegion>());

    int asyncRecordedTimePeriods(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot);

	/** 
     * Get \a camera params. 
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int httpStatusCode, const QList<QPair<QString, QVariant> > &params)</tt>.
     * \a status is 0 in case of success, in other cases it holds error code 
     * 
     * \returns                         Request handle.
	 */
    int asyncGetParamList(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int getParamList(const QnNetworkResourcePtr &camera, const QStringList &keys, QnStringVariantPairList *reply);

	/** 
	 * Set \a camera params.
	 * 
	 * Returns immediately. On request completion \a slot of object \a target is 
	 * called with signature <tt>(int httpStatusCode, const QList<QPair<QString, bool> > &operationResult)</tt>
	 * \a status is 0 in case of success, in other cases it holds error code
	 * 
     * \returns                         Request handle.
	 */
    int asyncSetParam(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int setParamList(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QnStringBoolPairList *reply);

    /** 
     * \returns                         Request handle. 
     */
    int asyncGetStatistics(QObject *target, const char *slot);

    // TODO: #GDM consistency! All other methods accept a single SLOT with signature (status, DATA, handle). Use a single slot here too!
    int asyncManualCameraSearch(const QString &startAddr, const QString &endAddr, const QString& username, const QString &password, const int port,
                                   QObject *target, const char *slotSuccess, const char *slotError); 

    int asyncManualCameraAdd(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password,
                                QObject *target, const char *slot);

    int asyncPtzMove(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int asyncPtzStop(const QnNetworkResourcePtr &camera, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int asyncPtzMoveTo(const QnNetworkResourcePtr &camera, const QVector3D &pos, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int asyncPtzGetPos(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);
    int asyncPtzGetSpaceMapper(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int asyncGetStorageSpace(QObject *target, const char *slot);

    int asyncGetStorageStatus(const QString &storageUrl, QObject *target, const char *slot);

    int asyncGetTime(QObject *target, const char *slot);

    QString getUrl() const { return m_url.toString(); }

    using base_type::connect;
    static bool connect(QnMediaServerReplyProcessor *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType connectionType = Qt::AutoConnection);

protected:
    QnRequestParamList createParamList(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion> &motionRegions);
    static QnRequestParamList createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params);
    static QnRequestParamList createSetParamsRequest(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params);

private:
    int recordedTimePeriods(const QnRequestParamList &params, QnTimePeriodList &timePeriodList, QByteArray &errorString);
    int asyncRecordedTimePeriods(const QnRequestParamList &params, QObject *target, const char *slot);

    int sendAsyncRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot);
    int sendSyncRequest(int object, const QnRequestParamList &params, QVariant *reply);

    template<class T>
    int sendSyncRequest(int object, const QnRequestParamList &params, T *reply) {
        assert(reply);

        QVariant replyVariant;
        int status = sendSyncRequest(object, params, &replyVariant);
        *reply = replyVariant.value<T>();
        return status;
    }

private:
    QScopedPointer<QnEnumNameMapper> m_nameMapper;
    QUrl m_url;
    QString m_proxyAddr;
    int m_proxyPort;
};


#endif // __VIDEO_SERVER_CONNECTION_H_
