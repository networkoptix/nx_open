#ifndef ABSTRACT_STREAM_DATA_PROVIDER
#define ABSTRACT_STREAM_DATA_PROVIDER

#include <nx/utils/thread/long_runnable.h>
#include <core/resource/resource_consumer.h>
#include <nx/streaming/abstract_data_packet.h>
#include <core/resource/resource_media_layout.h>
#include <utils/common/from_this_to_shared.h>
#include <core/dataprovider/abstract_video_camera.h>

class QnAbstractStreamDataProvider;
class QnResource;
class QnAbstractDataReceptor;

#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10)

struct AVCodecContext;

class QN_EXPORT QnAbstractStreamDataProvider : public QnLongRunnable, public QnResourceConsumer
{
    Q_OBJECT
public:
    explicit QnAbstractStreamDataProvider(const QnResourcePtr& resource);
    virtual ~QnAbstractStreamDataProvider();

    virtual bool dataCanBeAccepted() const;

    int processorsCount() const;
    void addDataProcessor(QnAbstractDataReceptor* dp);
    void removeDataProcessor(QnAbstractDataReceptor* dp);

    virtual bool isReverseMode() const { return false;}

    bool isConnectedToTheResource() const;

    void setNeedSleep(bool sleep);

    double getSpeed() const;

    virtual void disconnectFromResource() override;

    /* One resource may have several providers used with different roles*/
    virtual void setRole(Qn::ConnectionRole role);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const { return QnConstResourceVideoLayoutPtr(); }
    virtual bool hasVideo() const { return true; }
    virtual bool needConfigureProvider() const;
    virtual void startIfNotRunning(){ start(); }
    virtual QnSharedResourcePointer<QnAbstractVideoCamera> getOwner() const { return QnSharedResourcePointer<QnAbstractVideoCamera>();}

signals:
    void videoParamsChanged(AVCodecContext * codec);
    void slowSourceHint();
    void videoLayoutChanged();
protected:
    virtual void putData(const QnAbstractDataPacketPtr& data);
    void beforeDisconnectFromResource();

protected:
    QList<QnAbstractDataReceptor*> m_dataprocessors;
    mutable QnMutex m_mutex;
    QHash<QByteArray, QVariant> m_streamParam;
    Qn::ConnectionRole m_role;
};

typedef QSharedPointer<QnAbstractStreamDataProvider> QnAbstractStreamDataProviderPtr;

#endif // ABSTRACT_STREAM_DATA_PROVIDER
