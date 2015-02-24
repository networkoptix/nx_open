#ifndef QNSCREENMANAGER_H
#define QNSCREENMANAGER_H

#include <QtCore/QSharedMemory>
#include <QtCore/QRect>
#include <QtCore/QTimer>

class QnScreenManager : public QObject {
    Q_OBJECT
public:
    QnScreenManager(QObject *parent = 0);
    ~QnScreenManager();

    QSet<int> usedScreens() const;
    QSet<int> instanceUsedScreens() const;

    void updateCurrentScreens(const QWidget *widget);
    int nextFreeScreen() const;

private:
    void setCurrentScreens(const QSet<int> &screens);

private slots:
    void at_timer_timeout();
    void at_refreshTimer_timeout();

private:
    mutable QSharedMemory m_sharedMemory;
    int m_index;

    QTimer *m_refreshDelayTimer;
    QRect m_geometry;
};

#endif // QNSCREENMANAGER_H
