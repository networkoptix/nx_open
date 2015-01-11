#ifndef QNSCREENMANAGER_H
#define QNSCREENMANAGER_H

#include <QtCore/QSharedMemory>

class QnScreenManager : public QObject {
    Q_OBJECT
public:
    QnScreenManager(QObject *parent = 0);
    ~QnScreenManager();

    QSet<int> usedScreens();
    void setCurrentScreens(const QSet<int> &screens);

private slots:
    void at_timer_timeout();

private:
    QSharedMemory m_sharedMemory;
    int m_index;
};

#endif // QNSCREENMANAGER_H
