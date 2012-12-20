#ifndef stream_reader_514
#define stream_reader_514

#include "utils/common/long_runnable.h"
#include "../resource/resource_consumer.h"
#include "../datapacket/abstract_data_packet.h"
#include "../resource/param.h"
#include "../dataconsumer/abstract_data_consumer.h"

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

    //enum QnStreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

    explicit QnAbstractStreamDataProvider(QnResourcePtr resource);
    virtual ~QnAbstractStreamDataProvider();

    virtual bool dataCanBeAccepted() const;

    void addDataProcessor(QnAbstractDataReceptor* dp);
    void removeDataProcessor(QnAbstractDataReceptor* dp);

    virtual void setReverseMode(bool value, qint64 currentTimeHint = AV_NOPTS_VALUE) { Q_UNUSED(value); Q_UNUSED(currentTimeHint); }
    virtual bool isReverseMode() const { return false;}

    bool isConnectedToTheResource() const;

    void setNeedSleep(bool sleep);

    double getSpeed() const;

    void disconnectFromResource();

signals:
    void videoParamsChanged(AVCodecContext * codec);
    void slowSourceHint();

protected:
    virtual void putData(QnAbstractDataPacketPtr data);
    void beforeDisconnectFromResource();

protected:
    QList<QnAbstractDataReceptor*> m_dataprocessors;
    mutable QMutex m_mutex;
    QHash<QByteArray, QVariant> m_streamParam;
};

#endif //stream_reader_514
