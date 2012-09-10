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
#include <api/video_server_statistics_data.h>

#include "api_fwd.h"

class VideoServerSessionManager;

namespace detail {
    class VideoServerSimpleReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSimpleReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle);

    signals:
        void finished(int status, int handle);
    };

    class VideoServerSessionManagerReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString, int handle);

    signals:
        void finished(int status, const QnTimePeriodList& timePeriods, int handle);
    };

    class VideoServerSessionManagerFreeSpaceRequestReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerFreeSpaceRequestReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &errorString,int handle);

    signals:
        void finished(int status, qint64 freeSpace, qint64 usedSpace, int handle);
    };

    class VideoServerSessionManagerStatisticsRequestReplyProcessor: public QObject
    {
        Q_OBJECT;
    public:
        VideoServerSessionManagerStatisticsRequestReplyProcessor(QObject *parent = NULL): QObject(parent) {
            qRegisterMetaType<QnStatisticsDataList>("QnStatisticsDataList");
        }
    public slots:
        void at_replyReceived(int status, const QByteArray &reply, const QByteArray &/*errorString */ , int /*handle*/);
    signals:
        void finished(const QnStatisticsDataList &/* usage data */);
    };

    //!Handles response on GetParam request
    class VideoServerGetParamReplyProcessor: public QObject
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
    class VideoServerSetParamReplyProcessor: public QObject
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
}

class QN_EXPORT QnVideoServerConnection: public QObject
{
    Q_OBJECT;
public:
    QnVideoServerConnection(const QUrl &url, QObject *parent = 0);
    virtual ~QnVideoServerConnection();

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

    static void setProxyAddr(const QString &addr, int port);
    static int getProxyPort() { return m_proxyPort; }
    static QString getProxyHost() { return m_proxyAddr; }

    /** 
     * \returns                         Request handle. 
     */
    int asyncGetStatistics(QObject *target, const char *slot);

    /** 
     * \returns                         Status. 
     */
    int syncGetStatistics(QObject *target, const char *slot);

    int asyncPtzMove(const QnNetworkResourcePtr &camera, qreal xSpeed, qreal ySpeed, qreal zoomSpeed, QObject *target, const char *slot);
    int asyncPtzStop(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

private:
    int recordedTimePeriods(const QnRequestParamList &params, QnTimePeriodList &timePeriodList, QByteArray &errorString);

    int asyncRecordedTimePeriods(const QnRequestParamList &params, QObject *target, const char *slot);

protected:
    QnRequestParamList createParamList(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion> &motionRegions);

private:
    QUrl m_url;
    static QString m_proxyAddr;
    static int m_proxyPort;
};


#endif // __VIDEO_SERVER_CONNECTION_H_
