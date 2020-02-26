#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class BaseFigure: public ValueItem
{
    Q_OBJECT
public:
    BaseFigure(const QString& figureType, QObject* parent = nullptr);
    virtual void setValue(const QVariant& value) override;

private:
    static QJsonObject mergeFigures(
        const QJsonObject& currentFigureValue,
        const QJsonObject& newFigureValue);
};

class BoxFigure: public BaseFigure
{
public:
    BoxFigure(QObject* parent = nullptr);
};

class PolyFigure: public BaseFigure
{
    Q_OBJECT
    Q_PROPERTY(int minPoints READ minPoints WRITE setMinPoints NOTIFY minPointsChanged)
    Q_PROPERTY(int maxPoints READ maxPoints WRITE setMaxPoints NOTIFY maxPointsChanged)

    using base_type = BaseFigure;

public:
    using BaseFigure::BaseFigure;

    int minPoints() const { return m_minPoints; }
    void setMinPoints(int minPoints);

    int maxPoints() const { return m_maxPoints; }
    void setMaxPoints(int maxPoints);

    virtual QJsonObject serialize() const override;

signals:
    void minPointsChanged();
    void maxPointsChanged();

private:
    int m_minPoints = 0;
    int m_maxPoints = 0;
};

class LineFigure: public PolyFigure
{
    Q_OBJECT
    Q_PROPERTY(QString allowedDirections MEMBER m_allowedDirections)

public:
    LineFigure(QObject* parent = nullptr);

    virtual QJsonObject serialize() const override;

private:
    QString m_allowedDirections;
};

class PolygonFigure: public PolyFigure
{

public:
    PolygonFigure(QObject* parent = nullptr);
};

class ObjectSizeConstraints: public ValueItem
{
    Q_OBJECT

public:
    ObjectSizeConstraints(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
