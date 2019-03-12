#pragma once

#include <QtCore/QObject>
#include <utils/common/connective.h>

class QnCameraChunkProvider;

// TODO: #ynikitenkov Move it to the nx::vms::client::mobile namespace in 19.2
namespace nx {
namespace client {
namespace mobile {

class ChunkPositionWatcher: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

    Q_PROPERTY(bool motionSearchMode
        READ motionSearchMode
        WRITE setMotionSearchMode
        NOTIFY motionSearchModeChanged)

    Q_PROPERTY(qint64 position
        READ position
        WRITE setPosition
        NOTIFY positionChanged)

    Q_PROPERTY(QnCameraChunkProvider* chunkProvider
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

    QnCameraChunkProvider* chunkProvider() const;
    void setChunkProvider(QnCameraChunkProvider* value);

    Q_INVOKABLE qint64 nextChunkStartTimeMs() const;
    Q_INVOKABLE qint64 prevChunkStartTimeMs() const;

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
