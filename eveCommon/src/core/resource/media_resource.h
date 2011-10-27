#ifndef QnMediaResource_h_227
#define QnMediaResource_h_227

#include <QMap>
#include <QSize>


#include "core/resource/resource.h"

class QnAbstractMediaStreamDataProvider;
class QnDeviceVideoLayout;
class QnDeviceAudioLayout;

enum QnStreamQuality {QnQualityLowest, QnQualityLow, QnQualityNormal, QnQualityHigh, QnQualityHighest, QnQualityPreSeted};

class QnMediaResource : virtual public QnResource
{
public:
    QnMediaResource();
    virtual ~QnMediaResource();

    virtual QnDeviceVideoLayout* getVideoLayout(QnAbstractMediaStreamDataProvider* reader);
    virtual QnDeviceAudioLayout* getAudioLayout(QnAbstractMediaStreamDataProvider* reader);

    // size - is size of one channel; we assume all channels have the same size
    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(const QSize& size) const { return QnQualityNormal; }

    // returns one image best for such time
    // in case of live video time should be ignored
    virtual QImage getImage(int channnel, QDateTime time, QnStreamQuality quality);

    virtual int getStreamDataProvidersMaxAmount() const = 0;

    // all stream providers belong to this class;
    // on other words this class owns all stream providers
    // do not delete them

    // if MediaProvider with such number does not exists it will be created;
    // number belongs to the range [0; getStreamDataProvidersMaxAmount-1];
    QnAbstractMediaStreamDataProvider* acquireMediaProvider(int number, ConnectionRole role);

    // call the function if MediaProvider with such number is not going to be used any more till acquireMediaProvider is called again
    void releaseMediaProvider(int number);

    // if MediaProvider with such number does not => 0 pointer is returned
    QnAbstractMediaStreamDataProvider* getMediaProvider(int number);

    // create new instance of mediaProvider (so, ammount of providers is unlimited. It is used for Archive)
    QnAbstractMediaStreamDataProvider* createMediaProvider(ConnectionRole role);

protected:
    typedef QMap<int, QnAbstractMediaStreamDataProvider*> StreamProvidersList;
    StreamProvidersList m_streamProviders;
};

typedef QSharedPointer<QnMediaResource> QnMediaResourcePtr;


#endif // QnMediaResource_h_227
