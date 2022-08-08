// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CloudCrossSystemContext;

/**
 * Monitors available Cloud Systems and manages corresponding cross-system Contexts.
 */
class CloudCrossSystemManager: public QObject
{
    Q_OBJECT

public:
    explicit CloudCrossSystemManager(QObject* parent = nullptr);
    virtual ~CloudCrossSystemManager() override;

    QStringList cloudSystems() const;
    CloudCrossSystemContext* systemContext(const QString& systemId) const;

signals:
    void systemFound(const QString& systemId);
    void systemLost(const QString& systemId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
