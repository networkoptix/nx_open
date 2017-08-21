#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ExportOverlayWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ExportOverlayWidget(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& value);

    int textWidth() const;
    void setTextWidth(int value);

    QImage image() const;
    void setImage(const QImage& value);
    void setImage(const QPixmap& value);

    qreal scale() const;
    void setScale(qreal value);

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
    QScopedPointer<Private> d;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
