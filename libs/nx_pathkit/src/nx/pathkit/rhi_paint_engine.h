// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <mutex>

#include <QtGui/QPaintEngine>

#include "include/core/SkPath.h"

class QRhi;
class QRhiResourceUpdateBatch;
class QRhiRenderPassDescriptor;
class QRhiCommandBuffer;

namespace nx::pathkit {

class RhiPaintDevice;

struct NX_PATHKIT_API PaintPath
{
    QPen pen;
    QBrush brush;
    SkPath path;
    bool aa = false;
    qreal opacity = 1.0;
    std::optional<SkPath> clip;

    ~PaintPath();
};

struct PaintPixmap
{
    QRectF dst;
    QPixmap pixmap;
    QRectF src;
    QTransform transform;
    std::optional<SkPath> clip;
    qreal opacity = 1.0;
};

struct PaintCustom
{
    std::function<void(
        QRhi*,
        QRhiRenderPassDescriptor*,
        QRhiResourceUpdateBatch*,
        const QMatrix4x4& deviceMvp)> prepare;

    std::function<void(QRhi*, QRhiCommandBuffer*, QSize)> render;
};

using PaintData = std::variant<PaintPath, PaintPixmap, PaintCustom>;

/**
 * Synchronized data container for paint engine entries. It is shared between RhiPaintEngine and
 * RhiPaintDeviceRenderer and allows to submit paint data from the engine to the render (which may
 * reside in a separate thread in case of QML threaded rendering). It also tracks whether the data
 * has been rendered to avoid processing it multiple times.
 */
class RhiPaintEngineSyncData
{
public:
    /**
     * RAII class to lock the data for reading. Allows to iterate over the entries submitted to the
     * paint engine since last clear() call. On destruction, unlocks the data and marks it as
     * rendered to avoid processing the same data again.
     */
    class Entries
    {
        friend class RhiPaintEngineSyncData;

        Entries(RhiPaintEngineSyncData* parent): m_parent(parent)
        {
            m_parent->m_mutex.lock();
            if (m_parent->m_rendered)
                std::exchange(m_parent, nullptr)->m_mutex.unlock();
        }

    public:
        Entries(const Entries&) = delete;
        Entries& operator=(const Entries&) = delete;

        Entries(Entries&& other): m_parent(other.m_parent)
        {
            other.m_parent = nullptr;
        }

        Entries& operator=(Entries&& other)
        {
            if (this != &other)
            {
                unlock();
                m_parent = std::exchange(other.m_parent, nullptr);
            }
            return *this;
        }

        ~Entries() { unlock(); }

        /**
         * Returns true if current RhiPaintEngineSyncData has already been rendered.
         */
        bool isNull() const { return !m_parent; }

        /**
         * Returns all paint data entries submitted to the paint engine since last clear() call.
         */
        const QVector<PaintData>& all() const { return m_parent->m_entries; }

    private:
        void unlock()
        {
            if (!m_parent)
                return;
            m_parent->m_rendered = true;
            m_parent->m_mutex.unlock();
        }

        RhiPaintEngineSyncData* m_parent;
    };

    RhiPaintEngineSyncData();

    /**
     * Clears the data and marks it as not rendered.
     */
    void clear();

    /**
     * Appends a paint data entry to the list and marks the data as not rendered.
     */
    void append(PaintData&& p);

    /**
     * Appends a result of a function which generates a paint path to the list and marks the data
     * as not rendered. The function is called under a lock to avoid issues with implicit paths
     * re-allocations that may happen on renderer thread.
     */
    void append(std::function<PaintPath(SkPathRefAllocator*)> locked);

    /**
     * Locks the data for reading. Once the result object is destroyed, the data is considered as
     * rendered and any subsequent calls return a null object until the next clear() or append()
     * call.
     */
    Entries renderLock() { return Entries(this); }

private:
    mutable std::mutex m_mutex;
    bool m_rendered = false;
    QVector<PaintData> m_entries;
    std::unique_ptr<SkPathRefAllocator> m_allocator;
};

/**
 * A paint engine implementation which generates a list of drawing paths using Skia PathOps API.
 */
class NX_PATHKIT_API RhiPaintEngine: public QPaintEngine
{
    using base_type = QPaintEngine;

    friend class RhiPaintDevice;
    friend class RhiPaintDeviceRenderer;

public:
    RhiPaintEngine(RhiPaintDevice* device);

    bool begin(QPaintDevice *pdev) override;
    bool end() override;

    void updateState(const QPaintEngineState& state) override;
    Type type() const override;

    void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override;
    void drawPath(const QPainterPath& path) override;

    void drawPolygon(const QPointF* points, int pointCount, QPaintEngine::PolygonDrawMode mode) override;

    void drawCustom(const PaintCustom& custom);

private:
    void updateClipPath(const QPainterPath& clipPath, Qt::ClipOperation op);
    void updateClipPath(const SkPath& skPath, Qt::ClipOperation op);
    std::optional<SkPath> getClip() const;

    std::shared_ptr<RhiPaintEngineSyncData> m_data;
    RhiPaintDevice* m_device = nullptr;

    bool m_clipEnabled = false;
    SkPath m_clipPath;
};

} // namespace nx::pathkit
