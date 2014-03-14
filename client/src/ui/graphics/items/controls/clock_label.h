#ifndef QN_CLOCK_LABEL_H
#define QN_CLOCK_LABEL_H

#include <QtCore/QObject>

#include <ui/graphics/items/standard/graphics_label.h>

class QnClockDataProvider : public QObject {
    Q_OBJECT
public:
    explicit QnClockDataProvider(const QString fixedFormat = QString(), QObject *parent = 0);
    ~QnClockDataProvider();

signals:
    void timeChanged(const QString &timeString);

private slots:
    void at_timer_timeout();
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
};


class QnClockLabel: public GraphicsLabel {
    Q_OBJECT
    typedef GraphicsLabel base_type;

public:
    QnClockLabel(QGraphicsItem *parent = NULL);
    QnClockLabel(const QString &format, QGraphicsItem *parent = NULL);
    virtual ~QnClockLabel();

private:
    void init(const QString &format);
};

#endif // QN_CLOCK_LABEL_H
