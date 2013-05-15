#ifndef QN_MEDIA_SERVER_CONNECTION_H
#define QN_MEDIA_SERVER_CONNECTION_H

#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QVector3D>
#include <QtGui/QRegion>

#include <utils/common/request_param.h>
#include <utils/common/warnings.h>
#include <utils/common/util.h>

#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/time_reply.h>

#include <core/resource/resource_fwd.h>

#include <recording/time_period_list.h>

#include "api_fwd.h"
#include "abstract_connection.h"
#include "media_server_cameras_data.h"
#include "business/actions/abstract_business_action.h"
#include "business/events/abstract_business_event.h"

class QnPtzSpaceMapper;
class QnEnumNameMapper;

typedef QList<QPair<QString, bool> > QnStringBoolPairList;
typedef QList<QPair<QString, QVariant> > QnStringVariantPairList;

Q_DECLARE_METATYPE(QnStringBoolPairList);
Q_DECLARE_METATYPE(QnStringVariantPairList);


class QnMediaServerReplyProcessor: public QnAbstractReplyProcessor {
    Q_OBJECT

public:
    QnMediaServerReplyProcessor(int object): QnAbstractReplyProcessor(object) {}

    virtual void processReply(const QnHTTPRawResponse &response, int handle) override;

signals:
    void finished(int status, const QnStorageStatusReply &reply, int handle);
    void finished(int status, const QnStorageSpaceReply &reply, int handle);
    void finished(int status, const QnTimePeriodList &reply, int handle);
    void finished(int status, const QnStatisticsReply &reply, int handle);
    void finished(int status, const QnPtzSpaceMapper &reply, int handle);
    void finished(int status, const QVector3D &reply, int handle);
    void finished(int status, const QnStringVariantPairList &reply, int handle);
    void finished(int status, const QnStringBoolPairList &reply, int handle);
    void finished(int status, const QnTimeReply &reply, int handle);
    void finished(int status, const QnCamerasFoundInfoList &reply, int handle);
    void finished(int status, const QnLightBusinessActionVectorPtr &reply, int handle);

private:
    friend class QnAbstractReplyProcessor;
};


class QnMediaServerConnection: public QnAbstractConnection {
    Q_OBJECT
    typedef QnAbstractConnection base_type;

public:
    QnMediaServerConnection(const QUrl &mediaServerApiUrl, QObject *parent = NULL);
    virtual ~QnMediaServerConnection();

    void setProxyAddr(const QUrl &apiUrl, const QString &addr, int port);
    int getProxyPort() { return m_proxyPort; }
    QString getProxyHost() { return m_proxyAddr; }

    int getTimePeriodsAsync(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot);

	/** 
     * Get \a camera params. 
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int httpStatusCode, const QList<QPair<QString, QVariant> > &params)</tt>.
     * \a status is 0 in case of success, in other cases it holds error code 
     * 
     * \returns                         Request handle.
	 */
    int getParamsAsync(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot);

    // TODO: #Elric rename getEventLogAsync
	/** 
     * Get \a event log. 
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int handle, int httpStatusCode, const QList<QnAbstractBusinessAction> &events)</tt>.
     * \a status is 0 in case of success, in other cases it holds error code 
     * 
     * \param camRes filter events by camera. Optional.
     * \param dateFrom  start timestamp in msec
     * \param dateTo end timestamp in msec. Can be DATETIME_NOW
     * \param businessRuleId filter events by specified business rule. Optional.
     * 
     * \returns                         Request handle.
	 */
    int asyncEventLog(
        qint64 dateFrom, qint64 dateTo, 
        QnNetworkResourcePtr camRes, 
        BusinessEventType::Value eventType, 
        BusinessActionType::Value actionType,
        QnId businessRuleId, 
        QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int getParamsSync(const QnNetworkResourcePtr &camera, const QStringList &keys, QnStringVariantPairList *reply);

	/** 
	 * Set \a camera params.
	 * 
	 * Returns immediately. On request completion \a slot of object \a target is 
	 * called with signature <tt>(int httpStatusCode, const QList<QPair<QString, bool> > &operationResult)</tt>
	 * \a status is 0 in case of success, in other cases it holds error code
	 * 
     * \returns                         Request handle.
	 */
    int setParamsAsync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int setParamsSync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QnStringBoolPairList *reply);

    /** 
     * \returns                         Request handle. 
     */
    int getStatisticsAsync(QObject *target, const char *slot);

    int searchCameraAsync(const QString &startAddr, const QString &endAddr, const QString &username, const QString &password, int port, QObject *target, const char *slot); 
    int addCameraAsync(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password, QObject *target, const char *slot);

    int ptzMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzStopAsync(const QnNetworkResourcePtr &camera, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzMoveToAsync(const QnNetworkResourcePtr &camera, const QVector3D &pos, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzGetPosAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);
    int ptzGetSpaceMapperAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int getStorageSpaceAsync(QObject *target, const char *slot);

    int getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot);

    int getTimeAsync(QObject *target, const char *slot);

protected:
    virtual QnAbstractReplyProcessor *newReplyProcessor(int object) override;

    static QnRequestParamList createTimePeriodsRequest(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion> &motionRegions);
    static QnRequestParamList createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params);
    static QnRequestParamList createSetParamsRequest(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params);

private:
    QString m_proxyAddr;
    int m_proxyPort;
};


#endif // __VIDEO_SERVER_CONNECTION_H_
