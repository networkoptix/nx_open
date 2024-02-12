// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <nx/utils/uuid.h>

struct QnRoutePoint {
    nx::Uuid peerId;
    QString host;
    quint16 port;

    QnRoutePoint() {}

    QnRoutePoint(const nx::Uuid &peerId, const QString &host, quint16 port) :
        peerId(peerId), host(host), port(port)
    {}

    bool operator ==(const QnRoutePoint &other) const;
};

struct QnOldRoute
{
    QList<QnRoutePoint> points;
    int weight = 0;

    QnOldRoute() = default;

    bool isValid() const;
    bool isEqual(const QnOldRoute &other) const;

    int length() const;

    bool addPoint(const QnRoutePoint &point, int weight);
    bool containsConnection(const nx::Uuid &first, const nx::Uuid &from, const QnRoutePoint &point) const;
    bool containsPoint(const nx::Uuid &id) const;

    bool operator <(const QnOldRoute &other) const;
    QnOldRoute operator +(const QnOldRoute &other) const;
};

using QnRouteList = QList<QnOldRoute>;
