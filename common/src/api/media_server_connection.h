#ifndef __VIDEO_SERVER_CONNECTION_H_
#define __VIDEO_SERVER_CONNECTION_H_

#include <QUrl>
#include <QRegion>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QAuthenticator>

#include "core/resource/network_resource.h"
#include "utils/common/util.h"
#include "recording/time_period_list.h"

#include <utils/common/request_param.h>
#include <api/media_server_statistics_data.h>
#include <api/media_server_cameras_data.h>

#include "api_fwd.h"

namespace detail {
    class QnMediaServerSimpleReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        QnMediaServerSimpleReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle);

    signals:
        void finished(int status, int handle);
    };

    class QnMediaServerTimePeriodsReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerTimePeriodsReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle);

    signals:
        void finished(int status, const QnTimePeriodList& timePeriods, int handle);
    };

    class QnMediaServerFreeSpaceReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerFreeSpaceReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString,int handle);

    signals:
        void finished(int status, qint64 freeSpace, qint64 usedSpace, int handle);
    };

    class QnMediaServerStatisticsReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerStatisticsReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &/*errorString */ , int /*handle*/);

    signals:
        void finished(const QnStatisticsDataList &/* usage data */);
    };

    class QnMediaServerManualCameraReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerManualCameraReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_searchReplyReceived(int status, const QByteArray &reply, const QByteArray &errorString , int handle);
        void at_addReplyReceived(int status, const QByteArray &reply, const QByteArray &errorString , int handle);

    signals:
        void finishedSearch(const QnCamerasFoundInfoList &);
        void finishedAdd(int);
    };

    class QnMediaServerGetTimeReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        QnMediaServerGetTimeReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle);

    signals:
        void finished(int status, const QDateTime &dateTime, int utcOffset, int handle);
    };

    //!Handles response on GetParam request
    class QnMediaServerGetParamReplyProcessor: public QObject
    {
        Q_OBJECT

    public:
        //!Return value is actual only after response has been handled
        const QList< QPair< QString, QVariant> > &receivedParams() const;

        //!Parses response mesasge body and fills \a m_receivedParams
        void parseResponse(const QByteArray& responseMssageBody);

    public slots:
        /*!
            \note calls \a deleteLater after parsing response response
        */
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray /* &errorString */ , int /*handle*/);

    signals:
        void finished(int status, const QList< QPair< QString, QVariant> > &params);

    private:
        QList< QPair< QString, QVariant> > m_receivedParams;
    };

    //!Handles response on SetParam request
    class QnMediaServerSetParamReplyProcessor: public QObject
    {
        Q_OBJECT

    public:
        //!QList<QPair<paramName, operation result> >. Return value is actual only after response has been handled
        const QList<QPair<QString, bool> > &operationResult() const;
        //!Parses response mesasge body and fills \a m_receivedParams
        void parseResponse(const QByteArray &responseMssageBody);

    public slots:
        /*!
            \note calls \a deleteLater after handling response
        */
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray /* &errorString */ , int /*handle*/);

    signals:
        void finished(int status, const QList<QPair<QString, bool> > &operationResult);

    private:
         QList<QPair<QString, bool> > m_operationResult;
    };

} // namespace detail


class QN_EXPORT QnMediaServerConnection: public QObject
{
    Q_OBJECT
public:
    QnMediaServerConnection(const QUrl &url, QObject *parent = 0);
    virtual ~QnMediaServerConnection();

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
    int asyncGetParamList(const QnNetworkResourcePtr &camera, const QStringList &params, QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int getParamList(const QnNetworkResourcePtr &camera, const QStringList &params, QList<QPair<QString, QVariant> > *paramValues);

	/** 
	 * Set \a camera params.
	 * 
	 * Returns immediately. On request completion \a slot of object \a target is 
	 * called with signature <tt>(int httpStatusCode, const QList<QPair<QString, bool> > &operationResult)</tt>
	 * \a status is 0 in case of success, in other cases it holds error code
	 * 
     * \returns                         Request handle.
	 */
    int asyncSetParam(const QnNetworkResourcePtr &camera, const QList<QPair<QString, QVariant> > &params, QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int setParamList(const QnNetworkResourcePtr &camera, const QList<QPair<QString, QVariant> > &params, QList<QPair<QString, bool> > *operationResult);

    int asyncGetFreeSpace(const QString &path, QObject *target, const char *slot);

    void setProxyAddr(const QString &addr, int port);
    int getProxyPort() { return m_proxyPort; }
    QString getProxyHost() { return m_proxyAddr; }

    /** 
     * \returns                         Request handle. 
     */
    int asyncGetStatistics(QObject *target, const char *slot);

    /** 
     * \returns                         Status. 
     */
    int syncGetStatistics(QObject *target, const char *slot);

    // TODO: #gdm why 'Get'? This is not a get request.
    // TODO: #gdm (QObject *target, const char *slot) are normally the last parameter pair.
    int asyncGetManualCameraSearch(QObject *target, const char *slot,
                                   const QString &startAddr, const QString &endAddr, const QString& username, const QString &password, const int port);

    // TODO: #gdm why 'Get'? This is not a get request.
    // TODO: #gdm (QObject *target, const char *slot) are normally the last parameter pair.
    int asyncGetManualCameraAdd(QObject *target, const char *slot,
                                const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password);

    int asyncPtzMove(const QnNetworkResourcePtr &camera, qreal xSpeed, qreal ySpeed, qreal zoomSpeed, QObject *target, const char *slot);
    int asyncPtzStop(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int asyncGetTime(QObject *target, const char *slot);

protected:
    QnRequestParamList createParamList(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion> &motionRegions);

private:
    int recordedTimePeriods(const QnRequestParamList &params, QnTimePeriodList &timePeriodList, QByteArray &errorString);

    int asyncRecordedTimePeriods(const QnRequestParamList &params, QObject *target, const char *slot);

private:
    QUrl m_url;
    QString m_proxyAddr;
    int m_proxyPort;
};


#endif // __VIDEO_SERVER_CONNECTION_H_
