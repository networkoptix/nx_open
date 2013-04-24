#ifndef CLOCK_DATA_PROVIDER_H
#define CLOCK_DATA_PROVIDER_H

#include <QObject>
#include <QTimer>

class QnClockDataProvider : public QObject
{
    Q_OBJECT
public:
    explicit QnClockDataProvider(QObject *parent = 0);
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

    TimeFormat m_format;

    bool m_showWeekDay;
    bool m_showDateAndMonth;
    bool m_showSeconds;

    QTimer* m_timer;

    QString m_formatString;
};

#endif // CLOCK_DATA_PROVIDER_H
