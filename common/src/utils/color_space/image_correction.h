#ifndef __IMAGE_CORRECTION_H__
#define __IMAGE_CORRECTION_H__

struct ImageCorrectionParams
{
    ImageCorrectionParams():
        enabled(false),
        blackLevel(0.0),
        whiteLevel(0.0),
        gamma(1.0) {}
    bool enabled;
    float blackLevel;
    float whiteLevel;
    float gamma;
};

struct ImageCorrectionResult
{

    ImageCorrectionResult(): aCoeff(1.0), bCoeff(0.0), gamma(1.0) {     
        memset(hystogram, 0, sizeof(hystogram)); 
    }
    float aCoeff;
    float bCoeff;
    float gamma;
    int hystogram[256];

    void processImage( quint8* yPlane, int width, int height, int stride, const ImageCorrectionParams& data, const QRectF& srcRect);

    void reset();
private:
    float calcGamma(int leftPos, int rightPos, int pixels) const;
};

#endif // __IMAGE_CORRECTION_H__
