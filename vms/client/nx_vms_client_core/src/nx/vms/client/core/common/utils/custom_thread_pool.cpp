// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_thread_pool.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QCoreApplication>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

// ------------------------------------------------------------------------------------------------
// CustomThreadPool::Manager

class CustomThreadPool::Manager: public QObject
{
public:
    static CustomThreadPool* threadPool(const QString& id)
    {
        if (!NX_ASSERT(QCoreApplication::instance(), "Application must be created")
            || !NX_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread(),
                "Must be called from the main application thread"))
        {
            return nullptr;
        }

        if (QCoreApplication::instance()->closingDown())
            return nullptr;

        return instance()->get(id);
    }

private:
    Manager()
    {
        NX_CRITICAL(QCoreApplication::instance() && !QCoreApplication::instance()->closingDown());

        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
            &Manager::deinitialize, Qt::UniqueConnection);
    }

    virtual ~Manager() override
    {
        NX_ASSERT(std::all_of(m_instances.cbegin(), m_instances.cend(),
            [](const QPointer<CustomThreadPool>& instance) { return instance.isNull(); }),
            "CustomThreadPool::Manager destroyed without deinitialization");

        // We intentionally delete CustomThreadPool instances only on QCoreApplication::aboutToQuit
        // as they wait for threads to finish and need an event loop to complete.
        // Deleting them here when the application event loop is finished would cause
        // the application to freeze at exit.
    }

    CustomThreadPool* get(const QString& id)
    {
        if (!m_instances.contains(id))
            m_instances[id] = new CustomThreadPool(id);

        return m_instances[id];
    }

    void deinitialize()
    {
        for (const auto instance: m_instances)
            delete instance;
    }

    static Manager* instance()
    {
        static Manager manager;
        return &manager;
    }

private:
    QHash<QString, QPointer<CustomThreadPool>> m_instances;
};

// ------------------------------------------------------------------------------------------------
// CustomThreadPool

CustomThreadPool::CustomThreadPool(const QString& id)
{
    NX_CRITICAL(QCoreApplication::instance() && !QCoreApplication::instance()->closingDown());

    NX_VERBOSE(this, "Creating thread pool %1 (%2)", this, id);

    setObjectName(id);
}

CustomThreadPool::~CustomThreadPool()
{
    NX_VERBOSE(this, "Destroying thread pool %1 (%2)", this, objectName());
}

CustomThreadPool* CustomThreadPool::instance(const QString& id)
{
    return Manager::threadPool(id);
}

} // namespace nx::vms::client::core
