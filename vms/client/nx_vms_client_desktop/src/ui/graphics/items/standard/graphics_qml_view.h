#pragma once

#include <QGraphicsWidget>
#include <QQuickWidget>

class QQmlEngine;
class QQuickWindow;
class QQuickItem;
class QQmlError;
class QOpenGLWidget;

namespace nx::vms::client::desktop {

/**
 * Shows QML on scene using interface similar to QQuickWidget.
 */
class GraphicsQmlView : public QGraphicsWidget
{
    Q_OBJECT

    using base_type = QGraphicsWidget;

public:
    GraphicsQmlView(QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = 0);
    ~GraphicsQmlView();

    QQmlEngine* engine() const;
    void setData(const QByteArray& data, const QUrl& url);

    QQuickWindow* quickWindow() const;
    QQuickItem* rootObject() const;
    QList<QQmlError> errors() const;

public slots:
    void setSource(const QUrl& url);

signals:
    void statusChanged(QQuickWidget::Status status);

protected:
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

    virtual bool event(QEvent* event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void keyReleaseEvent(QKeyEvent* event) override;
    virtual void focusInEvent(QFocusEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
