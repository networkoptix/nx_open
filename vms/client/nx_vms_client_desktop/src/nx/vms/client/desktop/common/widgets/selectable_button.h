#pragma once

#include <QtWidgets/QToolButton>

#include <nx/vms/client/desktop/common/utils/custom_painted.h>

namespace nx::vms::client::desktop {

class SelectableButton: public CustomPainted<QToolButton>
{
    Q_OBJECT
    using base_type = CustomPainted<QToolButton>;

public:
    SelectableButton(QWidget* parent = nullptr);

    int markerFrameWidth() const;
    void setMarkerFrameWidth(int width);

    int markerMargin() const;
    void setMarkerMargin(int margin);

    qreal roundingRadius() const;
    void setRoundingRadius(int width);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    void updateContentsMargins();
    void paintMarker(QPainter* painter, const QBrush& brush);

private:
    int m_markerFrameWidth = 2;
    int m_markerMargin = 1;
    qreal m_roundingRadius = 2.0;
};

} // namespace nx::vms::client::desktop
