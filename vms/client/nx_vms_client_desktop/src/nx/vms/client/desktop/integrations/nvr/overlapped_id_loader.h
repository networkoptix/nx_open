// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop::integrations {

class OverlappedIdStore;

class OverlappedIdLoader:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    OverlappedIdLoader(
        SystemContext* systemContext,
        const QString& groupId,
        std::shared_ptr<OverlappedIdStore> store,
        QObject* parent = nullptr);
    virtual ~OverlappedIdLoader() override;

    void start();
    void stop();

private:
    void requestIdList();

private:
    QTimer m_loadingAttemptTimer;
    bool m_stopped = true;

    QString m_groupId;
    std::shared_ptr<OverlappedIdStore> m_store;
};

} // namespace nx::vms::client::desktop::integrations
