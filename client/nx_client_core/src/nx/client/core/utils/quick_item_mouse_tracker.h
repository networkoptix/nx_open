#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>

class QQuickItem;

namespace nx {
namespace client {
namespace core {

class QuickItemMouseTracker: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)
    Q_PROPERTY(qreal mouseX READ mouseX NOTIFY mouseXChanged)
    Q_PROPERTY(qreal mouseY READ mouseY NOTIFY mouseYChanged)
    Q_PROPERTY(QQuickItem* item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool containsMouse READ containsMouse NOTIFY containsMouseChanged)
    Q_PROPERTY(bool hoverEventsEnabled
        READ hoverEventsEnabled WRITE setHoverEventsEnabled
        NOTIFY hoverEventsEnabledChanged)

    using base_type = QObject;

public:
    QuickItemMouseTracker(QObject* parent = nullptr);
    virtual ~QuickItemMouseTracker() override;

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    QPointF position() const;
    qreal mouseX() const;
    qreal mouseY() const;

    bool containsMouse() const;

    bool hoverEventsEnabled() const;
    void setHoverEventsEnabled(bool enabled);

    QQuickItem* item() const;
    void setItem(QQuickItem* item);

    static void registerQmlType();

signals:
    void positionChanged();
    void mouseXChanged();
    void mouseYChanged();
    void containsMouseChanged();
    void hoverEventsEnabledChanged();
    void itemChanged();

private:
    void setMousePosition(const QPointF& pos);
    void setContainsMouse(bool containsMouse);

private:
    QQuickItem* m_item = nullptr;
    QPointF m_mousePosition;
    bool m_containsMouse = false;
    bool m_originalHoverEventsEnabled = false;
};

} // namespace core
} // namespace client
} // namespace nx
