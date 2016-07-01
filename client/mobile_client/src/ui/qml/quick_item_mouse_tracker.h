#pragma once

#include <QtQuick/QQuickItem>

class QnQuickItemMouseTracker : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(qreal mouseX READ mouseX NOTIFY mouseXChanged)
    Q_PROPERTY(qreal mouseY READ mouseY NOTIFY mouseYChanged)
    Q_PROPERTY(QQuickItem* item READ item WRITE setItem NOTIFY itemChanged)

public:
    QnQuickItemMouseTracker(QQuickItem* parent = nullptr);
    ~QnQuickItemMouseTracker();

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    qreal mouseX() const;
    qreal mouseY() const;

    QQuickItem* item() const;

public slots:
    void setItem(QQuickItem* item);

signals:
    void mouseXChanged();
    void mouseYChanged();
    void mousePositionChanged();
    void itemChanged();

private:
    void setMousePosition(const QPointF& pos);

private:
    QQuickItem* m_item;
    QPointF m_mousePosition;
};
