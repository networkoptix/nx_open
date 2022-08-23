// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <network/cloud_system_description.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>

#include "abstract_systems_finder.h"

class QTimer;

namespace nx { namespace network { namespace http { class AsyncHttpClientPtr; } } }

class QnCloudSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    QnCloudSystemsFinder(QObject* parent = nullptr);
    virtual ~QnCloudSystemsFinder() override;

public: // overrides
    SystemDescriptionList systems() const override;
    QnSystemDescriptionPtr getSystem(const QString& id) const override;

private:
    void onCloudStatusChanged(nx::vms::client::core::CloudStatusWatcher::Status status);
    void setCloudSystems(const QnCloudSystemList& systems);
    void pingCloudSystem(const QString& cloudSystemId);
    void updateSystems();
    void tryRemoveAlienServer(const nx::vms::api::ModuleInformation& serverInfo);
    void updateStateUnsafe(const QnCloudSystemList& targetSystems);

private:
    typedef QScopedPointer<QTimer> QTimerPtr;
    typedef QHash<QString, QnCloudSystemDescription::PointerType> SystemsHash;
    typedef QHash<int, QString> RequestIdToSystemHash;

    const QTimerPtr m_updateSystemsTimer;
    mutable nx::Mutex m_mutex;

    SystemsHash m_systems;
    QList<nx::network::http::AsyncHttpClientPtr> m_runningRequests;
};
