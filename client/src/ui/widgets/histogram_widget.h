#ifndef QN_HISTOGRAM_WIDGET_H
#define QN_HISTOGRAM_WIDGET_H

#include <QtGui/QWidget>

#include <utils/color_space/image_correction.h>


class QnHistogramWidget: public QWidget, public QnHistogramConsumer {
    Q_OBJECT
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

#endif // QN_HISTOGRAM_WIDGET_H
