#ifndef stream_reader_514
#define stream_reader_514

#ifdef ENABLE_DATA_PROVIDERS

#include "utils/common/long_runnable.h"
#include "../resource/resource_consumer.h"
#include "../datapacket/abstract_data_packet.h"
#include "../resource/param.h"
#include "../dataconsumer/abstract_data_consumer.h"
#include "../resource/resource_media_layout.h"

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
    bool needConfigureProvider() const;
signals:
    void videoParamsChanged(AVCodecContext * codec);
    void slowSourceHint();

protected:
    virtual void putData(const QnAbstractDataPacketPtr& data);
    void beforeDisconnectFromResource();

protected:
    QList<QnAbstractDataReceptor*> m_dataprocessors;
    mutable QnMutex m_mutex;
    QHash<QByteArray, QVariant> m_streamParam;
    Qn::ConnectionRole m_role;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //stream_reader_514
