// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_HISTOGRAM_WIDGET_H
#define QN_HISTOGRAM_WIDGET_H

#include <QtWidgets/QWidget>

#include <nx/vms/api/data/image_correction_data.h>

#include <utils/color_space/image_correction.h>

class QnHistogramWidget: public QWidget, public QnHistogramConsumer {
    Q_OBJECT
    typedef QWidget base_type;

public:
    QnHistogramWidget(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = {});

    virtual void setHistogramData(const ImageCorrectionResult &data) override;
    void setHistogramParams(const nx::vms::api::ImageCorrectionData &params);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    ImageCorrectionResult m_data;
    nx::vms::api::ImageCorrectionData m_params;
};

#endif // QN_HISTOGRAM_WIDGET_H
