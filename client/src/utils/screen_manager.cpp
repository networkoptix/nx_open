#include "screen_manager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include "utils/common/app_info.h"
#include "utils/common/process.h"

namespace {

    const int dataVersion = 0;
    const QString key = QnAppInfo::customizationName() + lit("/QnScreenManager/") + QString::number(dataVersion);
    const int maxClients = 64;
    const int checkDeadsInterval = 60 * 1000;

    struct ScreenUsageData {
        quint64 pid;
        quint64 screens;

        void setScreens(const QSet<int> &screens) {
            this->screens = 0;
            for (int screen: screens)
                this->screens |= 1 << screen;
        }

        QSet<int> getScreens() const {
            QSet<int> screens;
            for (int i = 0; i < static_cast<int>(sizeof(this->screens)); i++) {
                if (this->screens & (1ull << i))
                    screens.insert(i);
            }
            return screens;
        }
    };

} // anonymous namespace

QnScreenManager::QnScreenManager(QObject *parent) :
    QObject(parent),
    m_sharedMemory(key),
    m_index(-1)
{
    bool success = m_sharedMemory.create(maxClients * sizeof(ScreenUsageData));
    if (success) {
        m_sharedMemory.lock();
        memset(m_sharedMemory.data(), 0, maxClients * sizeof(ScreenUsageData));
        m_sharedMemory.unlock();
    }

    if (!success && m_sharedMemory.error() == QSharedMemory::AlreadyExists)
        success = m_sharedMemory.attach();

    if (success) {
        QTimer *timer = new QTimer(this);
        connnect(timer, &QTimer::timeout, this &QnScreenManager::at_timer_timeout);
        timer->start(checkDeadsInterval);
    }
}

QnScreenManager::~QnScreenManager() {
    if (m_index == -1)
        return;

    if (!m_sharedMemory.lock())
        return;

    ScreenUsageData *data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    data[m_index].pid = 0;
    data[m_index].screens = 0;
    m_index = -1;

    m_sharedMemory.unlock();
}

QSet<int> QnScreenManager::usedScreens() {
    QSet<int> result;

    if (!m_sharedMemory.lock())
        return result;

    ScreenUsageData *data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    for (int i = 0; i < maxClients && data[i].pid != 0; i++)
        result += data[i].getScreens();

    m_sharedMemory.unlock();

    return result;
}

void QnScreenManager::setCurrentScreens(const QSet<int> &screens) {
    if (!m_sharedMemory.lock())
        return;

    ScreenUsageData *data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    if (m_index == -1) {
        for (int i = 0; i < maxClients; i++) {
            if (data[i].pid == 0) {
                m_index = i;
                data[i].pid = QCoreApplication::applicationPid();
                break;
            }
        }
    }

    if (m_index >= 0)
        data[m_index].setScreens(screens);

    m_sharedMemory.unlock();
}

void QnScreenManager::at_timer_timeout() {
    if (!m_sharedMemory.lock())
        return;

    ScreenUsageData *data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    for (int i = 0; i < maxClients; i++) {
        if (data[i].pid == 0)
            continue;

        if (!nx::checkProcessExists(data[i].pid)) {
            data[i].pid = 0;
            data[i].screens = 0;
        }
    }

    m_sharedMemory.unlock();
}
