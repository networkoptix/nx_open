#pragma once

#include <QtWidgets/QLabel>

namespace nx {
namespace client {
namespace desktop {

class RoundPanelLabel: public QLabel
{
    Q_OBJECT
    using base_type = QLabel;

public:
    explicit RoundPanelLabel(QWidget* parent = nullptr);
    virtual ~RoundPanelLabel() override = default;

    QColor backgroundColor() const;
    QColor effectiveBackgroundColor() const;
    void setBackgroundColor(const QColor& value);
    void unsetBackgroundColor();

    qreal roundingRaduis() const;
    void setRoundingRadius(qreal value, bool autoContentsMargins = true);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    void setAutomaticContentsMargins();

private:
    QColor m_backgroundColor;
    qreal m_roundingRadius = 2.0;
};

} // namespace desktop
} // namespace client
} // namespace nx
