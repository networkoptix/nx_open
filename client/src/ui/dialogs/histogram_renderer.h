#ifndef __HISTOGRAM_RENDERER_H__
#define __HISTOGRAM_RENDERER_H__

#include "utils/color_space/image_correction.h"

// TODO: #Elric a widget, not a renderer!!!

class QnHistogramWidget: public QWidget, public QnHistogramConsumer {
public:
    QnHistogramWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual void setHistogramData(const ImageCorrectionResult &data) override;
    void setHistogramParams(const ImageCorrectionParams &params);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    ImageCorrectionResult m_data;
    ImageCorrectionParams m_params;
};

#endif // __HISTOGRAM_RENDERER_H__
