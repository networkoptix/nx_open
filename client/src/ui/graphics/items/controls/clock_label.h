#ifndef QN_CLOCK_LABEL_H
#define QN_CLOCK_LABEL_H

#include <QtCore/QObject>
#include <QtWidgets/QMenu>

#include <ui/graphics/items/standard/graphics_label.h>

class QnClockDataProvider : public QObject {
    Q_OBJECT
public:
    enum ClockType
    {
        serverClock,
        localSystemClock
    };

    explicit QnClockDataProvider(const QString fixedFormat = QString(), QObject *parent = 0);
    ~QnClockDataProvider();

    void setClockType( ClockType clockType );

signals:
    void timeChanged(const QString &timeString);

private slots:
    void updateFormatString();

private:
    enum TimeFormat {
        Hour12,
        Hour24
    };

    QTimer *m_timer;
    TimeFormat m_format;
    bool m_showWeekDay;
    bool m_showDateAndMonth;
    bool m_showSeconds;
    QString m_formatString;
    ClockType m_clockType;
};


class QnClockLabel: public GraphicsLabel {
    Q_OBJECT
    typedef GraphicsLabel base_type;

public:
    QnClockLabel(QGraphicsItem *parent = NULL);
    QnClockLabel(const QString &format, QGraphicsItem *parent = NULL);
    virtual ~QnClockLabel();

protected:
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event ) override;

private:
    QAction* m_serverTimeAction;
    QAction* m_localTimeAction;
    QnClockDataProvider* m_provider;

    void init(const QString& format);
};

#endif // QN_CLOCK_LABEL_H
