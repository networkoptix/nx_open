// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ChunkProvider:
    public AbstractTimePeriodStorage,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = AbstractTimePeriodStorage;

    Q_PROPERTY(nx::Uuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(qint64 bottomBound READ bottomBound NOTIFY bottomBoundChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool loadingMotion READ isLoadingMotion NOTIFY loadingMotionChanged)
    Q_PROPERTY(QString motionFilter READ motionFilter WRITE setMotionFilter NOTIFY motionFilterChanged)

public:
    ChunkProvider(QObject* parent = nullptr);

    nx::Uuid resourceId() const;
    void setResourceId(const nx::Uuid& id);

    virtual const QnTimePeriodList& periods(Qn::TimePeriodContent type) const override;
    qint64 bottomBound() const;

    QString motionFilter() const;
    void setMotionFilter(const QString& value);

    bool isLoading() const;
    bool isLoadingMotion() const;

    Q_INVOKABLE qint64 closestChunkEndMs(qint64 position, bool forward) const;
    Q_INVOKABLE void update();
    Q_INVOKABLE bool hasChunks() const;
    Q_INVOKABLE bool hasMotionChunks() const;

    static void registerQmlType();

signals:
    void resourceIdChanged();
    void bottomBoundChanged();
    void loadingChanged();
    void loadingMotionChanged();
    void motionFilterChanged();

private:
    void handleLoadingChanged(Qn::TimePeriodContent contentType);

private:
    class ChunkProviderInternal;
    using ChunkProviderPtr = QSharedPointer<ChunkProviderInternal>;
    using ProvidersHash = QHash<Qn::TimePeriodContent, ChunkProviderPtr>;
    const ProvidersHash m_providers;
};

} // namespace nx::vms::client::core
