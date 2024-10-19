// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

Q_MOC_INCLUDE("nx/vms/client/core/media/chunk_provider.h")

namespace nx::vms::client::core { class ChunkProvider; }

// TODO: #ynikitenkov Move it to the nx::vms::client::mobile namespace in 19.2
namespace nx {
namespace client {
namespace mobile {

class ChunkPositionWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool motionSearchMode
        READ motionSearchMode
        WRITE setMotionSearchMode
        NOTIFY motionSearchModeChanged)

    Q_PROPERTY(qint64 position
        READ position
        WRITE setPosition
        NOTIFY positionChanged)

    Q_PROPERTY(nx::vms::client::core::ChunkProvider* chunkProvider
        READ chunkProvider
        WRITE setChunkProvider
        NOTIFY chunkProviderChanged)

public:
    static void registerQmlType();

    explicit ChunkPositionWatcher(QObject* parent = nullptr);
    virtual ~ChunkPositionWatcher() override;

    bool motionSearchMode() const;
    void setMotionSearchMode(bool value);

    qint64 position() const;
    void setPosition(qint64 value);

    nx::vms::client::core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(nx::vms::client::core::ChunkProvider* value);

    Q_INVOKABLE qint64 nextChunkStartTimeMs() const;
    Q_INVOKABLE qint64 prevChunkStartTimeMs() const;
    Q_INVOKABLE qint64 firstChunkStartTimeMs() const;

signals:
    void motionSearchModeChanged();
    void positionChanged();
    void chunkProviderChanged();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace mobile
} // namespace client
} // namespace nx
