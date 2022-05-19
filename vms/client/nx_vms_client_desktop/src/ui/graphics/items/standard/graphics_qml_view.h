// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>
#include <QtQuickWidgets/QQuickWidget>

class QQmlEngine;
class QQuickWindow;
class QQuickItem;
class QQmlError;
class QOpenGLWidget;

namespace nx::vms::client::desktop {

/**
 * Shows QML on scene using interface similar to QQuickWidget.
 */
class GraphicsQmlView: public QGraphicsWidget
{
    Q_OBJECT

    using base_type = QGraphicsWidget;

public:
    GraphicsQmlView(QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = {});
    ~GraphicsQmlView();

    QQmlEngine* engine() const;
    void setData(const QByteArray& data, const QUrl& url);

    QQuickWindow* quickWindow() const;
    QQuickItem* rootObject() const;
    QList<QQmlError> errors() const;

    /** Detach root object from this view. */
    std::shared_ptr<QQuickItem> takeRootObject();

    /** Set new root object for this view. */
    void setRootObject(std::shared_ptr<QQuickItem> root);

public slots:
    void setSource(const QUrl& url);
    void updateWindowGeometry();

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
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    virtual void dropEvent(QGraphicsSceneDragDropEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void keyReleaseEvent(QKeyEvent* event) override;
    virtual void focusInEvent(QFocusEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;
    virtual bool focusNextPrevChild(bool next) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
