#ifndef __HISTOGRAM_RENDERER_H__
#define __HISTOGRAM_RENDERER_H__

#include <QWidget>

#include "utils/color_space/image_correction.h"

// TODO: #Elric a widget, not a renderer!!!

class QnHistogramRenderer: public QWidget, public QnHistogramConsumer
{
public:
    QnHistogramRenderer(QWidget* parent);

    virtual void setHistogramData(const ImageCorrectionResult& data) override;
    void setHistogramParams(const ImageCorrectionParams& params);
protected:
    virtual void paintEvent( QPaintEvent * event ) override;
private:
    ImageCorrectionResult m_data;
    ImageCorrectionParams m_params;
};

#endif // __HISTOGRAM_RENDERER_H__
