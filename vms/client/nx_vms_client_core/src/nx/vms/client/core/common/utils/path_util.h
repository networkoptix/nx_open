#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtGui/QPainterPath>

namespace nx::vms::client::core {

class PathUtil: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points WRITE setPoints NOTIFY pathChanged)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY pathChanged)

public:
    PathUtil(QObject* parent = nullptr);

    QVariantList points();
    void setPoints(const QVariantList& points);

    QRectF boundingRect() const;
    Q_INVOKABLE bool contains(const QPointF& point) const;

    Q_INVOKABLE bool checkSelfIntersections() const;

    static void registerQmlType();

signals:
    void pathChanged();

private:
    void updatePath();

private:
    QVector<QPointF> m_points;
    QPainterPath m_path;
};

} // namespace nx::vms::client::core
