#ifndef QnMediaResource_h_227
#define QnMediaResource_h_227

#include <QMap>
#include <QSize>


#include "core/resource/resource.h"
#include "resource_media_layout.h"

class QnAbstractMediaStreamDataProvider;
class QnVideoResourceLayout;
class QnResourceAudioLayout;

enum QnStreamQuality {QnQualityLowest, QnQualityLow, QnQualityNormal, QnQualityHigh, QnQualityHighest, QnQualityPreSeted};

class QnMediaResource : virtual public QnResource
{
public:
    QnMediaResource();
    virtual ~QnMediaResource();

    // size - is size of one channel; we assume all channels have the same size
    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(const QSize& /*size*/) const { return QnQualityNormal; }

    // returns one image best for such time
    // in case of live video time should be ignored
    virtual QImage getImage(int channnel, QDateTime time, QnStreamQuality quality) const;

    // resource can use DataProvider for addition info (optional)
    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0);
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0);
protected:
    CLCustomDeviceVideoLayout* m_customVideoLayout;
};

typedef QSharedPointer<QnMediaResource> QnMediaResourcePtr;


#endif // QnMediaResource_h_227
