// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

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

    Q_PROPERTY(Qn::TimePeriodContent contentType
        READ contentType
        WRITE setContentType
        NOTIFY contentTypeChanged)

    Q_PROPERTY(qint64 position
        READ position
        WRITE setPosition
        NOTIFY positionChanged)

    Q_PROPERTY(bool playingLive
        READ playingLive
        WRITE setPlayingLive
        NOTIFY playingLiveChanged)

    Q_PROPERTY(nx::vms::client::core::ChunkProvider* chunkProvider
        READ chunkProvider
        WRITE setChunkProvider
        NOTIFY chunkProviderChanged)

    Q_PROPERTY(qint64 firstChunkMs READ firstChunkStartTimeMs NOTIFY chunksChanged)
    Q_PROPERTY(qint64 prevChunkMs READ prevChunkStartTimeMs NOTIFY chunksChanged)
    Q_PROPERTY(qint64 nextChunkMs READ nextChunkStartTimeMs NOTIFY chunksChanged)

public:
    static void registerQmlType();

    explicit ChunkPositionWatcher(QObject* parent = nullptr);
    virtual ~ChunkPositionWatcher() override;

    Qn::TimePeriodContent contentType() const;
    void setContentType(Qn::TimePeriodContent value);

    qint64 position() const;
    void setPosition(qint64 value);

    bool playingLive() const;
    void setPlayingLive(bool value);

    nx::vms::client::core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(nx::vms::client::core::ChunkProvider* value);

    Q_INVOKABLE qint64 nextChunkStartTimeMs() const;
    Q_INVOKABLE qint64 prevChunkStartTimeMs() const;
    Q_INVOKABLE qint64 firstChunkStartTimeMs() const;

    /**
     * @return The position shifted from the current position by the given offset in milliseconds,
     *     considering recorded content only, skipping gaps between chunks, -1 if the shifted
     *     position reaches live, DATETIME_INVALID if it goes before the first chunk.
     */
    Q_INVOKABLE qint64 positionShiftedBy(qint64 offsetMs) const;

signals:
    void contentTypeChanged();
    void positionChanged();
    void playingLiveChanged();
    void chunkProviderChanged();
    void chunksChanged();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace mobile
} // namespace client
} // namespace nx
