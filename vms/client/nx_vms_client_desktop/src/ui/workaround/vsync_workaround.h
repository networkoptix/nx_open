#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

/**
 *  This class ensures that update() events are passed to widget
 *  no more often than once in a 16ms (~60 fps)
  **/
class QnVSyncWorkaround : public QObject {
    Q_OBJECT
public:
    QnVSyncWorkaround(QObject *watched, QObject *parent = NULL);

    bool eventFilter(QObject *object, QEvent *event);

private slots:
    void updateWatchedWidget();

private:
    QObject *m_watched;
    QEvent *m_updateEventToPass;
    QElapsedTimer m_elapsedTimer;
};
