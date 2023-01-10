// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QtGlobal>
#include <QtGui/QImage>

namespace nx::vms::client::desktop::workbench::timeline {

/**
 * Value class representing a single video thumbnail.
 */
class Thumbnail
{
public:
    Thumbnail(
        std::chrono::milliseconds time,
        std::chrono::milliseconds actualTime,
        int generation)
        :
        m_time(time),
        m_actualTime(actualTime),
        m_generation(generation)
    {
    }

    Thumbnail(
        const QImage& image,
        const QSize& size,
        std::chrono::milliseconds time,
        std::chrono::milliseconds actualTime,
        int generation)
        :
        m_image(image),
        m_size(size),
        m_time(time),
        m_actualTime(actualTime),
        m_generation(generation)
    {
    }

    Thumbnail():
        m_time(0),
        m_actualTime(0)
    {
    }

    Thumbnail(const Thumbnail& other, std::chrono::milliseconds time):
        m_image(other.m_image),
        m_size(other.m_size),
        m_time(time),
        m_actualTime(other.m_actualTime),
        m_generation(other.m_generation)
    {
    }

    bool isEmpty() const
    {
        return m_image.isNull();
    }

    QImage image() const
    {
        return m_image;
    }

    QSize size() const
    {
        return m_size;
    }

    std::chrono::milliseconds time() const
    {
        return m_time;
    }

    std::chrono::milliseconds actualTime() const
    {
        return m_actualTime;
    }

    int generation() const
    {
        return m_generation;
    }

private:
    QImage m_image;
    QSize m_size;
    std::chrono::milliseconds m_time;
    std::chrono::milliseconds m_actualTime;
    int m_generation;
};

using ThumbnailPtr = std::shared_ptr<const Thumbnail>;

} // namespace nx::vms::client::desktop::workbench::timeline
