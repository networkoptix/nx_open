#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QMenu>

#include <ui/graphics/items/standard/graphics_label.h>

class QnClockDataProvider: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum ClockType
    {
        serverClock,
        localSystemClock
    };

    explicit QnClockDataProvider(QObject* parent = nullptr);
    virtual ~QnClockDataProvider() override;

    void setClockType(ClockType clockType);

signals:
    void timeChanged(const QString& timeString);

private:
    QTimer* m_timer;
    ClockType m_clockType;
};


class QnClockLabel: public GraphicsLabel
{
    Q_OBJECT
    using base_type = GraphicsLabel;

public:
    QnClockLabel(QGraphicsItem* parent = nullptr);
    virtual ~QnClockLabel() override;

protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    QAction* m_serverTimeAction;
    QAction* m_localTimeAction;
    QnClockDataProvider* m_provider;
};
