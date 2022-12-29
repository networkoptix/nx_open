// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class ScalableImageWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ScalableImageWidget(const QString& pixmapPath, QWidget* parent = nullptr);

    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

private:
    QString m_pixmapPath;
    QPixmap m_pixmap;
    qreal m_aspectRatio = 0.0;
};

} // namespace nx::vms::client::desktop
