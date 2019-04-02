#include "delaying_proxy_image_provider.h"

#include <QtCore/QLinkedList>
#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

struct DelayingProxyImageProvider::Manager
{
    static Manager& instance()
    {
        static Manager manager;
        return manager;
    }

    Manager(): pendingOperation(new nx::utils::PendingOperation())
    {
        pendingOperation->setIntervalMs(ini().tilePreviewLoadIntervalMs);
        pendingOperation->setFlags(nx::utils::PendingOperation::FireImmediately);
        pendingOperation->setCallback(
            [this]() { loadOne(); });
    }

    void requestLoad(DelayingProxyImageProvider* provider)
    {
        queuedProviders.removeAll(provider);
        queuedProviders.removeAll(nullptr);

        queuedProviders.push_back(provider);

        while (queuedProviders.size() > maximumQueueLength)
            queuedProviders.pop_front();

        pendingOperation->requestOperation();
    }

    void loadOne()
    {
        queuedProviders.removeAll(nullptr);
        if (queuedProviders.empty())
            return;

        queuedProviders.front()->ProxyImageProvider::doLoadAsync();
        queuedProviders.pop_front();

        if (!queuedProviders.empty())
            pendingOperation->requestOperation();
    }

    int maximumQueueLength = 25;
    QLinkedList<QPointer<DelayingProxyImageProvider>> queuedProviders;
    nx::utils::ImplPtr<nx::utils::PendingOperation> pendingOperation;
};

milliseconds DelayingProxyImageProvider::minimumDelay()
{
    return Manager::instance().pendingOperation->interval();
}

void DelayingProxyImageProvider::setMinimumDelay(milliseconds value)
{
    Manager::instance().pendingOperation->setInterval(value);
}

int DelayingProxyImageProvider::maximumQueueLength()
{
    return Manager::instance().maximumQueueLength;
}

void DelayingProxyImageProvider::setMaximumQueueLength(int value)
{
    Manager::instance().maximumQueueLength = value;
}

void DelayingProxyImageProvider::doLoadAsync()
{
    Manager::instance().requestLoad(this);
}

} // namespace nx::vms::client::desktop
