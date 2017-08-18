#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class OverlayLabelWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    OverlayLabelWidget(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& value);

    bool wordWrap() const;
    void setWordWrap(bool value);

    QPixmap pixmap() const;
    void setPixmap(const QPixmap& value);

    bool borderVisible() const;
    void setBorderVisible(bool value);

    qreal roundingRadius() const;
    void setRoundingRadius(qreal value);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual bool event(QEvent* event) override;

private:
    void updateLayout();
    void updateCursor();
    void updatePosition(const QPoint& pos);

private:
    struct Private;
    class FrameWidget;
    QScopedPointer<Private> d;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
