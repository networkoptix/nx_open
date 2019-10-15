#pragma once

#include <QGraphicsWidget>
#include <QQuickWindow>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QOpenGLWidget>
#include <QTimer>


namespace nx::vms::client::desktop {

class RenderControl;

class GraphicsQmlView : public QGraphicsWidget
{
    Q_OBJECT

    using base_type = QGraphicsWidget;

public:
    GraphicsQmlView(QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = 0);
    ~GraphicsQmlView();

    QQmlEngine* engine() const;
    void setData(const QByteArray& data, const QUrl& url);

    QQuickWindow* quickWindow() { return m_quickWindow; }
    QQuickItem* rootObject() const { return m_rootItem; }
    QList<QQmlError> errors() const { return m_qmlComponent->errors(); }

public slots:
    void setSource(const QUrl& url);

signals:
    void statusChanged(QQmlComponent::Status status);

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

private slots:
    void updateSizes();

private:
    void ensureFbo();
    void scheduleUpdateSizes();
    void componentStatusChanged(QQmlComponent::Status status);
    void initialize(const QRectF& rect, QOpenGLWidget* glWidget);

    bool m_initialized;
    QQuickWindow* m_quickWindow;
    QQuickItem* m_rootItem;
    RenderControl* m_renderControl;
    QOffscreenSurface* m_offscreenSurface;
    QOpenGLFramebufferObject* m_fbo;
    QQmlComponent* m_qmlComponent;
    QQmlEngine* m_qmlEngine;
    QTimer* m_resizeTimer;
};

} // namespace nx::vms::client::desktop
