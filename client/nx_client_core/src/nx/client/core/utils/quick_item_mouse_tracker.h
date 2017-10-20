#pragma once

#include <QtQuick/QQuickItem>

namespace nx {
namespace client {
namespace core {

class QuickItemMouseTracker: public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(qreal mouseX READ mouseX NOTIFY mouseXChanged)
    Q_PROPERTY(qreal mouseY READ mouseY NOTIFY mouseYChanged)
    Q_PROPERTY(QQuickItem* item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool containsMouse READ containsMouse NOTIFY containsMouseChanged)
    Q_PROPERTY(bool hoverEventsEnabled
        READ hoverEventsEnabled WRITE setHoverEventsEnabled
        NOTIFY hoverEventsEnabledChanged)

public:
    QuickItemMouseTracker(QQuickItem* parent = nullptr);
    virtual ~QuickItemMouseTracker() override;

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    qreal mouseX() const;
    qreal mouseY() const;

    bool containsMouse() const;

    bool hoverEventsEnabled() const;
    void setHoverEventsEnabled(bool enabled);

    QQuickItem* item() const;
    void setItem(QQuickItem* item);

    static void registerQmlType();

signals:
    void mouseXChanged();
    void mouseYChanged();
    void mousePositionChanged();
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
