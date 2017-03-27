#include "client_instance_manager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <QtWidgets/QDesktopWidget>

#include <client/client_settings.h>

#include "utils/common/app_info.h"
#include "utils/common/process.h"

namespace {

const int dataVersion = 1;
const QString key = QnAppInfo::customizationName() + lit("/QnClientInstanceManager/") + QString::number(dataVersion);
const int maxClients = 256;
const int checkDeadsInterval = 60 * 1000;
const int refreshDelay = 1000;

struct InstanceData
{
    quint64 pid = 0;
    bool settingsDirty = false;
};

struct QnSharedMemoryLocker
{
public:
    QnSharedMemoryLocker(QSharedMemory *memory):
        m_memory(memory),
        m_locked(memory->lock())
    {
        NX_ASSERT(m_locked, Q_FUNC_INFO, "Could not initialize shared memory");
        if (!m_locked)
            qWarning() << "Could not initialize shared memory";
    }

    ~QnSharedMemoryLocker()
    {
        if (!m_locked)
            return;
        m_memory->unlock();
    }

    bool isValid() const
    {
        return m_locked;
    }

private:
    QSharedMemory* m_memory;
    const bool m_locked;

};

} // anonymous namespace

QnClientInstanceManager::QnClientInstanceManager(QObject *parent):
    QObject(parent),
    m_sharedMemory(key),
    m_index(-1),
    m_instanceGuid(QnUuid::createUuid())
{
    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull())
    {
        pcUuid = QnUuid::createUuid();
        qnSettings->setPcUuid(pcUuid);
    }

    bool success = m_sharedMemory.create(maxClients * sizeof(InstanceData));
    if (success)
    {
        QnSharedMemoryLocker lock(&m_sharedMemory);
        success = lock.isValid();
        if (success)
            memset(m_sharedMemory.data(), 0, maxClients * sizeof(InstanceData));
    }

    if (!success && m_sharedMemory.error() == QSharedMemory::AlreadyExists)
        success = m_sharedMemory.attach();

    NX_ASSERT(success, Q_FUNC_INFO, "Could not initialize shared memory");
    if (!success)
    {
        qWarning() << "Could not initialize shared memory";
        return;
    }

    QTimer *checkDeadTimer = new QTimer(this);
    connect(checkDeadTimer, &QTimer::timeout, this, [this]
    {
        QnSharedMemoryLocker lock(&m_sharedMemory);
        if (!lock.isValid())
            return;

        InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
        for (int i = 0; i < maxClients; i++)
        {
            if (data[i].pid == 0)
                continue;

            if (!nx::checkProcessExists(data[i].pid))
            {
                data[i].pid = 0;
                data[i].settingsDirty = false;
            }
        }

        m_sharedMemory.unlock();
    });
    checkDeadTimer->start(checkDeadsInterval);

    {
        QnSharedMemoryLocker lock(&m_sharedMemory);
        if (!lock.isValid())
            return;

        InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
        for (int i = 0; i < maxClients; i++)
        {
            if (data[i].pid == 0)
            {
                m_index = i;
                data[i].pid = QCoreApplication::applicationPid();
                m_instanceGuid = instanceGuidForIndex(m_index);
                break;
            }
        }
    }

}

QnClientInstanceManager::~QnClientInstanceManager()
{
    if (m_index == -1)
        return;

    QnSharedMemoryLocker lock(&m_sharedMemory);
    if (!lock.isValid())
        return;

    InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
    data[m_index].pid = 0;
    data[m_index].settingsDirty = false;
    m_index = -1;
}

int QnClientInstanceManager::instanceIndex() const
{
    return m_index;
}

QnUuid QnClientInstanceManager::instanceGuid() const
{
    return m_instanceGuid;
}

QnUuid QnClientInstanceManager::instanceGuidForIndex(int index) const
{
    QnUuid pcUuid = qnSettings->pcUuid();
    NX_ASSERT(!pcUuid.isNull(), Q_FUNC_INFO, "pcUuid must already be created in class constructor");
    if (pcUuid.isNull())
    {
        pcUuid = QnUuid::createUuid();
        qnSettings->setPcUuid(pcUuid);
    }

    return QnUuid::createUuidFromPool(pcUuid.getQUuid(), index);
}


bool QnClientInstanceManager::isOwnSettingsDirty() const
{
    if (!isValid())
        return false;

    QnSharedMemoryLocker lock(&m_sharedMemory);
    if (!lock.isValid())
        return false;

    InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
    return data[m_index].settingsDirty;
}

void QnClientInstanceManager::markOtherSettingsDirty(bool value)
{
    if (!isValid())
        return;

    QnSharedMemoryLocker lock(&m_sharedMemory);
    if (!lock.isValid())
        return;

    InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
    for (int i = 0; i < maxClients; i++)
    {
        if (i == m_index)
            continue;

        if (data[i].pid == 0)
            continue;

        data[i].settingsDirty = value;
    }
}

void QnClientInstanceManager::markOwnSettingsDirty(bool value)
{
    if (!isValid())
        return;

    QnSharedMemoryLocker lock(&m_sharedMemory);
    if (!lock.isValid())
        return;

    InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
    data[m_index].settingsDirty = value;
}

bool QnClientInstanceManager::isValid() const
{
    return m_index >= 0;
}

QList<int> QnClientInstanceManager::runningInstancesIndices() const
{
    QList<int> result;

    QnSharedMemoryLocker lock(&m_sharedMemory);
    if (!lock.isValid())
        return result;

    InstanceData *data = reinterpret_cast<InstanceData*>(m_sharedMemory.data());
    for (int i = 0; i < maxClients; i++)
    {
        if (data[i].pid == 0)
            continue;
        result << i;
    }

    return result;
}
