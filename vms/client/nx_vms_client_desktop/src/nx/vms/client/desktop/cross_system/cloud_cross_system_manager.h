// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <network/base_system_description.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CloudCrossSystemContext;

/**
 * Monitors available Cloud Systems and manages corresponding cross-system Contexts. Note that some
 * connections may be deferred.
 */
class CloudCrossSystemManager: public QObject
{
    Q_OBJECT

public:
    explicit CloudCrossSystemManager(QObject* parent = nullptr);
    virtual ~CloudCrossSystemManager() override;

    QStringList cloudSystems() const;

    /**
     * Returns System context, forcing a connection if needed.
     */
    CloudCrossSystemContext* systemContext(const QString& systemId);

    /**
     * Returns System description.
     */
    const QnBaseSystemDescription* systemDescription(const QString& systemId) const;

    /**
     * Checks if the System context is currently available (some connections may be deferred).
     */
    bool isSystemContextAvailable(const QString& systemId) const;

    /**
     * Checks if the connection to the system was deferred.
     */
    bool isConnectionDeferred(const QString& systemId) const;

    /**
     * Requests System Context without forcing a connection. Can be used in places that do not
     * require a context immediately.
     *
     * @param systemId System id.
     * @param callback Function, which will be called once CloudCrossSystemContext is ready (or
     *     immediately).
     * @return Connection, if the context at the current moment is unavailable, which can be used
     *     to cancel the request through QObject::disconnect. Otherwise an empty connection is
     *     returned.
     */
    [[nodiscard]]
    QMetaObject::Connection requestSystemContext(
        const QString& systemId,
        std::function<void(CloudCrossSystemContext*)> callback);

signals:
    /** Emitted when System is found. */
    void systemFound(const QString& systemId);

    /** Emitted when System is loaded and its context can be used. */
    void systemContextReady(const QString& systemId);

    /** Emitted when System is lost. */
    void systemLost(const QString& systemId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
