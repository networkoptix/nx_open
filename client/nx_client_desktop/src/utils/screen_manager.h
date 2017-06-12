#pragma once

#include <QtCore/QSharedMemory>
#include <QtCore/QRect>
#include <QtCore/QTimer>

struct ScreenUsageData
{
    quint64 pid;
    quint64 screens;

    ScreenUsageData();
    void setScreens(const QSet<int> &screens);
    QSet<int> getScreens() const;
};

class QnScreenManager : public QObject {
    Q_OBJECT
public:
    QnScreenManager(QObject *parent = 0);
    ~QnScreenManager();

    QSet<int> usedScreens() const;
    QSet<int> instanceUsedScreens() const;

    void updateCurrentScreens(const QWidget *widget);
    int nextFreeScreen() const;

    bool isInitialized() const;

private:
    void setCurrentScreens(const QSet<int> &screens);

private slots:
    void at_timer_timeout();
    void at_refreshTimer_timeout();

private:
    mutable QSharedMemory m_sharedMemory;
    ScreenUsageData m_localData; // fallback when shared memory is not accessible
    int m_index;

    QTimer *m_refreshDelayTimer;
    QRect m_geometry;
    bool m_initialized;
};
