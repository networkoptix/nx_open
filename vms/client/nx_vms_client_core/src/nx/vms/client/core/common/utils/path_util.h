#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtGui/QPainterPath>

namespace nx::vms::client::core {

class PathUtil: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ pointsVariantList WRITE setPointsVariantList
        NOTIFY pathChanged)
    Q_PROPERTY(bool closed READ isClosed WRITE setClosed NOTIFY pathChanged)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY pathChanged)
    Q_PROPERTY(qreal length READ length NOTIFY pathChanged)
    Q_PROPERTY(QPointF midAnchorPoint READ midAnchorPoint NOTIFY pathChanged)
    Q_PROPERTY(qreal midAnchorPointNormalAngle READ midAnchorPointNormalAngle NOTIFY pathChanged)

public:
    PathUtil(QObject* parent = nullptr);

    QVector<QPointF> points() const;
    void setPoints(const QVector<QPointF>& points);

    QVariantList pointsVariantList() const;
    void setPointsVariantList(const QVariantList& points);

    bool isClosed() const;
    void setClosed(bool closed);

    QRectF boundingRect() const;
    Q_INVOKABLE bool contains(const QPointF& point) const;

    qreal length() const;
    QPointF midAnchorPoint() const;
    qreal midAnchorPointNormalAngle() const;

    Q_INVOKABLE bool checkSelfIntersections() const;

    static void registerQmlType();

signals:
    void pathChanged();

private:
    void updatePath();
    void calculateMidAnchorPoint();

private:
    QVector<QPointF> m_points;
    QPainterPath m_path;
    bool m_closed = false;
    qreal m_length = 0;
    QPointF m_midAnchorPoint;
    qreal m_midAnchorPointNormalAngle = 0;
};

} // namespace nx::vms::client::core
