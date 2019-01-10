#pragma once

#include <QtCore/QtGlobal>
#include <QtCore/QRectF>

namespace nx::vms::api { struct ImageCorrectionData; }

struct ImageCorrectionResult
{
    float aCoeff = 1.0f;
    float bCoeff = 0.0f;
    float gamma = 1.0f;
    int hystogram[256];
    bool filled = false;

    void analyseImage(
        const quint8* yPlane,
        int width,
        int height,
        int stride,
        const nx::vms::api::ImageCorrectionData& data,
        const QRectF& srcRect = QRectF(0.0, 0.0, 1.0, 1.0));
private:
    float calcGamma(int leftPos, int rightPos, int pixels) const;
};

class QnHistogramConsumer
{
public:
    virtual ~QnHistogramConsumer() {}
    virtual void setHistogramData(const ImageCorrectionResult& data) = 0;
};
