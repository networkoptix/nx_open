#ifndef QnMediaResource_h_227
#define QnMediaResource_h_227

#include "resource.h"
#include "dataprovider/media_streamdataprovider.h"
#include "datapacket/mediadatapacket.h"

class QnAbstractMediaStreamDataProvider;
class QnVideoResoutceLayout;

class QnMediaResource : virtual public QnResource 
{

public:
	QnMediaResource();
    virtual ~QnMediaResource();

    virtual QnVideoResoutceLayout* getVideoLayout() const;

    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(QSize size) = 0;
  
    // returns one image best for such time 
    // in case of live video time should be ignored 
    virtual QnCompressedVideoDataPtr getImage(int channnel, QDateTime time, QnStreamQuality quality) = 0;

    virtual int getStreamDataProvidersMaxAmount() const = 0;
    
    // all stream providers belong to this class; 
    // on other words this class owns all stream providers
    // do not delete them 

    // if MediaProvider with such number does not exists it will be created; 
    // number belongs to the range [0; getStreamDataProvidersMaxAmount-1];
    QnAbstractMediaStreamDataProvider* acquireMediaProvider(int number);

    // if MediaProvider with such number does not => 0 pointer is returned
    QnAbstractMediaStreamDataProvider* getMediaProvider(int number);
    
protected:
    virtual QnAbstractMediaStreamDataProvider* createMediaProvider() = 0;
protected:

    typedef QMap<int, QnAbstractMediaStreamDataProvider*> StreamProvidersList;
    StreamProvidersList m_streamProviders;
};

typedef QSharedPointer<QnMediaResource> QnMediaResourcePtr;


#endif // QnMediaResource_h_227
