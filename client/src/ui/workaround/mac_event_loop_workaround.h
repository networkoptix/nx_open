#ifndef MAC_EVENT_LOOP_WORKAROUND_H
#define MAC_EVENT_LOOP_WORKAROUND_H

#include <QObject>

class QnMacEventLoopWorkaround : public QObject {
    Q_OBJECT
public:
    QnMacEventLoopWorkaround(QObject *watched, QObject *parent = NULL);

    bool eventFilter(QObject *object, QEvent *event);

private slots:
    void updateWatchedWidget();

private:
    QObject *m_watched;
    QEvent *m_updateEventToPass;
};

#endif // MAC_EVENT_LOOP_WORKAROUND_H
