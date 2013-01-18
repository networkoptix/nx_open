#ifndef QN_MEDIA_RESOURCE_H
#define QN_MEDIA_RESOURCE_H

#include <QMap>
#include <QSize>
#include "resource.h"
#include "resource_media_layout.h"
#include "media_stream_quality.h"

class QnAbstractMediaStreamDataProvider;
class QnResourceVideoLayout;
class QnResourceAudioLayout;

QString QnStreamQualityToString(QnStreamQuality value);

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
    virtual QImage getImage(int channel, QDateTime time, QnStreamQuality quality) const;

    // resource can use DataProvider for addition info (optional)
    virtual const QnResourceVideoLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0);
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0);
protected:
    QnCustomResourceVideoLayout* m_customVideoLayout;
};

#endif // QN_MEDIA_RESOURCE_H
