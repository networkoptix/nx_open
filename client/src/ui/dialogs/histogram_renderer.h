#ifndef __HISTOGRAM_RENDERER_H__
#define __HISTOGRAM_RENDERER_H__

#include "utils/color_space/image_correction.h"

class QnHistogramRenderer: public QWidget, public QnHistogramConsumer
{
public:
    QnHistogramRenderer(QWidget* parent);

    virtual void setHistogramData(const QByteArray& data, double aCoeff, double bCoeff) override;
    void setHistogramParams(const ImageCorrectionParams& params);
protected:
    virtual void paintEvent( QPaintEvent * event ) override;
private:
    QByteArray m_histogramData;
    ImageCorrectionParams m_params;
    double m_aCoeff;
    double m_bCoeff;
};

#endif // __HISTOGRAM_RENDERER_H__
