#pragma once

#include <algorithm>
#include <vector>

#include <QtCore/QRectF>
#include <QtCore/QSize>

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

class TrackSerializer
{
public:
    template<typename T>
    static QByteArray serialized(const T& data)
    {
        QByteArray buf;
        serialize(data, &buf);
        return buf;
    }

    /**
     * Concatenated results of TrackSerializer::serialized are allowed.
     * So, all data from the buf is deserialized.
     */
    template<typename T>
    static T deserialized(const QByteArray& serializedData)
    {
        QnByteArrayConstRef buf(serializedData);
        T data;
        deserialize(&buf, &data);
        return data;
    }

private:
    /**
     * NOTE: Two serialization results can be appended to each other safely.
     */
    static void serialize(
        const std::vector<ObjectPosition>& track,
        QByteArray* buf);

    static void serializeTrackSequence(
        const std::vector<ObjectPosition>& track,
        QByteArray* buf);

    static void serialize(
        qint64 baseTimestamp,
        const ObjectPosition& position,
        QByteArray* buf);

    static void serialize(const QRect& rect, QByteArray* buf);

    static void deserialize(
        QnByteArrayConstRef* buf,
        std::vector<ObjectPosition>* track);

    static void deserializeTrackSequence(
        QnByteArrayConstRef* buf,
        std::vector<ObjectPosition>* track);

    static void deserialize(
        qint64 baseTimestamp,
        QnByteArrayConstRef* buf,
        ObjectPosition* position);

    static void deserialize(QnByteArrayConstRef* buf, QRect* rect);

    static void serialize(const QRectF& rect, QByteArray* buf);

    static void deserialize(QnByteArrayConstRef* buf, QRectF* rect);
};

//-------------------------------------------------------------------------------------------------

/**
 * Translates rect coordinates from [0; 1] to [0; resolution.width()] and [0; resolution.height()].
 */
QRect translate(const QRectF& box, const QSize& resolution);

/**
 * Translates rect coordinates from [0; resolution.width()] and [0; resolution.height()] to [0; 1].
 */
QRectF translate(const QRect& box, const QSize& resolution);

template<typename Rect>
struct OtherRect {};

template<>
struct OtherRect<QRect> { using type = QRectF; };

template<>
struct OtherRect<QRectF> { using type = QRect; };


template<template<typename...> class Container, typename Rect, typename... Args>
Container<typename OtherRect<Rect>::type> translate(
    const Container<Rect, Args...>& rects,
    const QSize& resolution)
{
    Container<typename OtherRect<Rect>::type> result;
    std::transform(
        rects.begin(), rects.end(),
        std::back_inserter(result),
        [&resolution](const Rect& rect) { return translate(rect, resolution); });
    return result;
}

} // namespace nx::analytics::storage
