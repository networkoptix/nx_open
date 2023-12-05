// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUuid>
#include <QtCore/QObject>

#include <functional>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

/**
 * Pushes requests to the queue and executes them not more than specified count simultaneously.
 */
class RequestQueue: public QObject
{
public:
    using RequestCallback = std::function<void ()>;
    using Request = std::function<void (const RequestCallback& callback)>;

    RequestQueue(int maxRunningRequestsCount = 1);
    ~RequestQueue();

    QUuid addRequest(const Request& request);
    bool removeRequest(const QUuid& id);
    void clear();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
