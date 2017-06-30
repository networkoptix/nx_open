#pragma once

#include <QtWidgets/QToolButton>

#include <ui/common/custom_painted.h>

class QnSelectableButton: public CustomPainted<QToolButton>
{
    Q_OBJECT
    using base_type = CustomPainted<QToolButton>;

public:
    QnSelectableButton(QWidget* parent = nullptr);

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
    int m_markerFrameWidth;
    int m_markerMargin;
    qreal m_roundingRadius;
};
