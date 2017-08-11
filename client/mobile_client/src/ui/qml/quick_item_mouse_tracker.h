#pragma once

#include <QtQuick/QQuickItem>

class QnQuickItemMouseTracker : public QQuickItem
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
    QnQuickItemMouseTracker(QQuickItem* parent = nullptr);
    ~QnQuickItemMouseTracker();

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    qreal mouseX() const;
    qreal mouseY() const;

    bool containsMouse() const;

    bool hoverEventsEnabled() const;
    void setHoverEventsEnabled(bool enabled);

    QQuickItem* item() const;

public slots:
    void setItem(QQuickItem* item);

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
