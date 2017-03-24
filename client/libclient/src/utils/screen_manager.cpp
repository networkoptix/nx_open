#include "screen_manager.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include "utils/common/app_info.h"
#include "utils/common/process.h"

namespace {

const int kDataVersion = 0;

// QRegExp is used internally to work with this key, so we cannot use lit macro here
const QString kMemoryAccessKey = QnAppInfo::customizationName() + QLatin1String("/QnScreenManager/") + QString::number(kDataVersion);

const int kMaxClients = 64;
const int kMemorySize = kMaxClients * sizeof(ScreenUsageData);
const int kCheckDeadsInterval = 60 * 1000;
const int kRefreshDelay = 1000;

} // namespace

ScreenUsageData::ScreenUsageData():
    pid(0),
    screens(0)
{
}

void ScreenUsageData::setScreens(const QSet<int> &screens)
{
    this->screens = 0;
    for (int screen: screens)
        this->screens |= 1ull << screen;
}

QSet<int> ScreenUsageData::getScreens() const
{
    QSet<int> screens;
    for (int i = 0; i < static_cast<int>(sizeof(this->screens)); i++)
    {
        if (this->screens & (1ull << i))
            screens.insert(i);
    }
    return screens;
}

QnScreenManager::QnScreenManager(QObject *parent) :
    QObject(parent),
    m_sharedMemory(kMemoryAccessKey),
    m_index(-1),
    m_refreshDelayTimer(new QTimer(this)),
    m_initialized(false)
{
    m_localData.pid = QCoreApplication::applicationPid();

    m_initialized = m_sharedMemory.create(kMemorySize);
    if (m_initialized)
    {
        m_sharedMemory.lock();
        memset(m_sharedMemory.data(), 0, kMemorySize);
        m_sharedMemory.unlock();
    }

    if (!m_initialized && m_sharedMemory.error() == QSharedMemory::AlreadyExists)
        m_initialized = m_sharedMemory.attach();

    if (m_initialized)
    {
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &QnScreenManager::at_timer_timeout);
        timer->start(kCheckDeadsInterval);
    }

    m_refreshDelayTimer->setSingleShot(true);
    m_refreshDelayTimer->setInterval(kRefreshDelay);
    connect(m_refreshDelayTimer, &QTimer::timeout, this, &QnScreenManager::at_refreshTimer_timeout);
}

QnScreenManager::~QnScreenManager()
{
    if (m_index == -1)
        return;

    if (!m_initialized || !m_sharedMemory.lock())
        return;

    ScreenUsageData* data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    data[m_index].pid = 0;
    data[m_index].screens = 0;
    m_index = -1;

    m_sharedMemory.unlock();
}

void QnScreenManager::updateCurrentScreens(const QWidget *widget)
{
    m_geometry = widget->geometry();
    m_refreshDelayTimer->start();
}

QSet<int> QnScreenManager::usedScreens() const
{
    if (!m_initialized || !m_sharedMemory.lock())
        return instanceUsedScreens();

    QSet<int> result;
    ScreenUsageData* data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    for (int i = 0; i < kMaxClients && data[i].pid != 0; i++)
        result += data[i].getScreens();

    m_sharedMemory.unlock();

    return result;
}

QSet<int> QnScreenManager::instanceUsedScreens() const
{
    return m_localData.getScreens();
}

void QnScreenManager::setCurrentScreens(const QSet<int> &screens)
{
    m_localData.setScreens(screens);

    if (!m_initialized || !m_sharedMemory.lock())
        return;

    ScreenUsageData* data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    if (m_index == -1)
    {
        for (int i = 0; i < kMaxClients; i++)
        {
            if (data[i].pid == 0)
            {
                m_index = i;
                data[i].pid = m_localData.pid;
                break;
            }
        }
    }

    if (m_index >= 0)
        data[m_index].setScreens(screens);

    m_sharedMemory.unlock();
}

int QnScreenManager::nextFreeScreen() const
{
    QSet<int> current = instanceUsedScreens();
    int currentScreen = current.isEmpty() ? 0 : *std::min_element(current.begin(), current.end());

    int screenCount = qApp->desktop()->screenCount();
    int nextScreen = (currentScreen + 1) % screenCount;

    QSet<int> used = usedScreens();
    for (int i = 0; i < screenCount; i++)
    {
        int screen = (nextScreen + i) % screenCount;
        if (!used.contains(screen))
            return screen;
    }

    return nextScreen;
}

bool QnScreenManager::isInitialized() const
{
    return m_initialized;
}

void QnScreenManager::at_timer_timeout()
{
    if (!m_initialized || !m_sharedMemory.lock())
        return;

    ScreenUsageData* data = reinterpret_cast<ScreenUsageData*>(m_sharedMemory.data());
    for (int i = 0; i < kMaxClients; i++)
    {
        if (data[i].pid == 0)
            continue;

        if (!nx::checkProcessExists(data[i].pid))
        {
            data[i].pid = 0;
            data[i].screens = 0;
        }
    }

    m_sharedMemory.unlock();
}

void QnScreenManager::at_refreshTimer_timeout()
{
    QSet<int> used;

    QDesktopWidget *desktop = qApp->desktop();

    int screenCount = desktop->screenCount();
    used.insert(desktop->screenNumber(m_geometry.center()));

    for (int i = 0; i < screenCount; i++)
    {
        QRect screenGeometry = desktop->screenGeometry(i);
        QRect intersected = screenGeometry.intersected(m_geometry);
        if (intersected.width() > screenGeometry.width() / 2 && intersected.height() > screenGeometry.height() / 2)
            used.insert(i);
    }

    setCurrentScreens(used);
}

