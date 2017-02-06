#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qset.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>

class QQuickItem;

class QnScenePositionListener:
    public QObject,
    public QQuickItemChangeListener
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* item READ item WRITE setItem)
    Q_PROPERTY(QPointF scenePos READ scenePos NOTIFY scenePosChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit QnScenePositionListener(QObject* parent = nullptr);
    ~QnScenePositionListener();

    QQuickItem* item() const { return m_item; }
    void setItem(QQuickItem* item);

    QPointF scenePos() const { return m_scenePos; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

signals:
    void scenePosChanged();
    void enabledChanged();

protected:
    void itemGeometryChanged(QQuickItem* item, const QRectF&, const QRectF&);
    void itemParentChanged(QQuickItem* item, QQuickItem* parent);
    void itemChildRemoved(QQuickItem* item, QQuickItem* child);
    void itemDestroyed(QQuickItem* item);

private:
    void updateScenePos();

    void removeAncestorListeners(QQuickItem* item);
    void addAncestorListeners(QQuickItem* item);

    bool isAncestor(QQuickItem* item) const;

    bool m_enabled = true;
    QPointF m_scenePos;
    QQuickItem* m_item = nullptr;
};
