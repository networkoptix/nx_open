#ifndef __IMAGE_CORRECTION_H__
#define __IMAGE_CORRECTION_H__

#include <QRectF>
#include <QByteArray>

struct ImageCorrectionParams
{
    bool operator== (const ImageCorrectionParams& other) const;

    ImageCorrectionParams():
        blackLevel(0.001f),
        whiteLevel(0.0005f),
        gamma(1.0f),
        enabled(false) {}

    static ImageCorrectionParams deserialize(const QByteArray& value)
    {
        ImageCorrectionParams result;
        QList<QByteArray> params = value.split(';');
        if (params.size() >= 4)
        {
            result.blackLevel = params[0].toDouble();
            result.whiteLevel = params[1].toDouble();
            result.gamma = params[2].toDouble();
            result.enabled = params[3].toInt();
        }
        return result;
    }

    QByteArray serialize() const
    {
        return QByteArray::number(blackLevel).append(';').append(QByteArray::number(whiteLevel)).append(';').
               append(QByteArray::number(gamma)).append(';').append(enabled ? '1' : '0').append((char)0);
    }

    float blackLevel;
    float whiteLevel;
    float gamma;
    bool enabled;
};

Q_DECLARE_METATYPE(ImageCorrectionParams);


struct ImageCorrectionResult
{

    ImageCorrectionResult(): aCoeff(1.0), bCoeff(0.0), gamma(1.0) {     
        memset(hystogram, 0, sizeof(hystogram)); 
    }
    float aCoeff;
    float bCoeff;
    float gamma;

    void analizeImage( const quint8* yPlane, int width, int height, int stride, const ImageCorrectionParams& data, const QRectF& srcRect);
    QByteArray getHystogram() const;
private:
    float calcGamma(int leftPos, int rightPos, int pixels) const;
    void clear();
private:
    //mutable QMutex m_mutex;
    int hystogram[256];
};

class QnHistogramConsumer
{
public:
    virtual ~QnHistogramConsumer() {}
    virtual void setHistogramData(const QByteArray& data, double aCoeff, double bCoeff) = 0;
};

#endif // __IMAGE_CORRECTION_H__
