#ifndef QN_MEDIA_RESOURCE_H
#define QN_MEDIA_RESOURCE_H

#include <QMap>
#include <QSize>
#include "resource.h"
#include "resource_media_layout.h"

class QnAbstractMediaStreamDataProvider;
class QnVideoResourceLayout;
class QnResourceAudioLayout;

enum QnStreamQuality {QnQualityLowest, QnQualityLow, QnQualityNormal, QnQualityHigh, QnQualityHighest, QnQualityPreSet};

class QnMediaResource : virtual public QnResource
{
    Q_OBJECT

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
    QnCustomDeviceVideoLayout* m_customVideoLayout;
};

#endif // QN_MEDIA_RESOURCE_H
