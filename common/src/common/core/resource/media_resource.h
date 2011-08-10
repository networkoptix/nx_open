#ifndef QnMediaResource_h_227
#define QnMediaResource_h_227

#include <QMap>
#include <QAudioFormat>

#include "dataprovider/media_streamdataprovider.h"
#include "datapacket/mediadatapacket.h"
#include "url_resource.h"

class QnAbstractMediaStreamDataProvider;
class QnMediaResourceLayout;

class QnMediaResource : virtual public QnResource
{

public:
	QnMediaResource();
    virtual ~QnMediaResource();

    virtual QnMediaResourceLayout* getMediaLayout() const;

    // size - is size of one channel; we assume all channels have the same size
    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(QSize size) const = 0;
  
    // returns one image best for such time 
    // in case of live video time should be ignored 
    virtual QnCompressedVideoDataPtr getImage(int channnel, QDateTime time, QnStreamQuality quality);

    virtual int getStreamDataProvidersMaxAmount() const = 0;
    
    // all stream providers belong to this class; 
    // on other words this class owns all stream providers
    // do not delete them 

    // if MediaProvider with such number does not exists it will be created; 
    // number belongs to the range [0; getStreamDataProvidersMaxAmount-1];
    QnAbstractMediaStreamDataProvider* acquireMediaProvider(int number);

    // call the function if MediaProvider with such number is not going to be used any more till acquireMediaProvider is called again
    void releaseMediaProvider(int number);

    // if MediaProvider with such number does not => 0 pointer is returned
    QnAbstractMediaStreamDataProvider* getMediaProvider(int number);

    // create new instance of mediaProvider (so, ammount of providers is unlimited. It is used for Archive)
    QnAbstractMediaStreamDataProvider* addMediaProvider();

protected:
    virtual QnAbstractMediaStreamDataProvider* createMediaProvider() = 0;
protected:

    typedef QMap<int, QnAbstractMediaStreamDataProvider*> StreamProvidersList;
    StreamProvidersList m_streamProviders;
};

typedef QSharedPointer<QnMediaResource> QnMediaResourcePtr;


#endif // QnMediaResource_h_227
