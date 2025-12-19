// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

#include <recording/time_period_list.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {
namespace timeline {

class NX_VMS_CLIENT_CORE_API ChunkBar: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QnTimePeriodList chunks READ chunks WRITE setChunks NOTIFY chunksChanged)
    Q_PROPERTY(qint64 startTimeMs READ startTimeMs WRITE setStartTimeMs NOTIFY startTimeChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs WRITE setDurationMs NOTIFY durationChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor
        NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor chunkColor READ chunkColor WRITE setChunkColor NOTIFY chunkColorChanged)
    Q_PROPERTY(bool mirrored READ mirrored WRITE setMirrored NOTIFY mirroredChanged)

public:
    explicit ChunkBar(QQuickItem* parent = nullptr);
    virtual ~ChunkBar() override;

    QnTimePeriodList chunks() const;
    void setChunks(const QnTimePeriodList& value);

    std::chrono::milliseconds startTime() const;
    void setStartTime(std::chrono::milliseconds value);
    qint64 startTimeMs() const;
    void setStartTimeMs(qint64 value);

    std::chrono::milliseconds duration() const;
    void setDuration(std::chrono::milliseconds value);
    qint64 durationMs() const;
    void setDurationMs(qint64 value);

    QColor backgroundColor() const;
    void setBackgroundColor(QColor value);

    QColor chunkColor() const;
    void setChunkColor(QColor value);

    bool mirrored() const;
    void setMirrored(bool value);

    static void registerQmlType();

signals:
    void chunksChanged();
    void startTimeChanged();
    void durationChanged();
    void backgroundColorChanged();
    void chunkColorChanged();
    void mirroredChanged();

protected:
    virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

    virtual QSGNode* updatePaintNode(
        QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::core::timeline
