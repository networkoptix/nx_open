#ifndef QN_HISTOGRAM_WIDGET_H
#define QN_HISTOGRAM_WIDGET_H

#include <QtWidgets/QWidget>

#include <client/client_color_types.h>

#include <utils/color_space/image_correction.h>

class QnHistogramWidget: public QWidget, public QnHistogramConsumer {
    Q_OBJECT
    Q_PROPERTY(QnHistogramColors colors READ colors WRITE setColors)
    typedef QWidget base_type;

public:
    QnHistogramWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);

    const QnHistogramColors &colors() const;
    void setColors(const QnHistogramColors &colors);

    virtual void setHistogramData(const ImageCorrectionResult &data) override;
    void setHistogramParams(const ImageCorrectionParams &params);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    ImageCorrectionResult m_data;
    ImageCorrectionParams m_params;
    QnHistogramColors m_colors;
};

#endif // QN_HISTOGRAM_WIDGET_H
