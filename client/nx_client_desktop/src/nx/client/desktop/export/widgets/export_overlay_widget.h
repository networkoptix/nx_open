#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

class QPixmapFilter;

namespace nx {
namespace client {
namespace desktop {

// TODO: #vkutin Remove all complicated painting, now duplicated
// in export media persistent settings structures (in createRuntimeSettings methods).
// Keep only image (for text, bookmark and image overlays) and timestamp painting.

class ExportOverlayWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ExportOverlayWidget(QWidget* parent = nullptr);
    virtual ~ExportOverlayWidget();

    QString text() const;
    void setText(const QString& value);

    int overlayWidth() const;
    void setOverlayWidth(int value);

    int textIndent() const;
    void setTextIndent(int value);

    QImage image() const;
    void setImage(const QImage& value);
    void setImage(const QPixmap& value);

    qreal scale() const;
    void setScale(qreal value);

    bool borderVisible() const;
    void setBorderVisible(bool value);

    qreal roundingRadius() const;
    void setRoundingRadius(qreal value);

    qreal opacity() const;
    void setOpacity(qreal value);

    bool hasShadow() const;
    void setShadow(const QColor& color, const QPointF& offset, qreal blurRadius, int iterations = 5);
    void removeShadow();

signals:
    void pressed();
    void released();

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
    void renderContent(QPainter& painter);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
