#pragma once

#include "object_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class BoxFigure: public ObjectValueItem
{
public:
    BoxFigure(QObject* parent = nullptr);
};

class PolyFigure: public ObjectValueItem
{
    Q_OBJECT
    Q_PROPERTY(int minPoints READ minPoints WRITE setMinPoints NOTIFY minPointsChanged)
    Q_PROPERTY(int maxPoints READ maxPoints WRITE setMaxPoints NOTIFY maxPointsChanged)
    Q_PROPERTY(bool useLabelField READ useLabelField WRITE setUseLabelField
        NOTIFY useLabelFieldChanged)

public:
    using ObjectValueItem::ObjectValueItem;

    int minPoints() const { return m_minPoints; }
    void setMinPoints(int minPoints);

    int maxPoints() const { return m_maxPoints; }
    void setMaxPoints(int maxPoints);

    bool useLabelField() const { return m_useLabelField; }
    void setUseLabelField(bool useLabelField);

    virtual QJsonObject serializeModel() const override;

signals:
    void minPointsChanged();
    void maxPointsChanged();
    void useLabelFieldChanged();

private:
    int m_minPoints = 0;
    int m_maxPoints = 0;
    bool m_useLabelField = true;
};

class LineFigure: public PolyFigure
{
    Q_OBJECT
    Q_PROPERTY(QString allowedDirections MEMBER m_allowedDirections)

public:
    LineFigure(QObject* parent = nullptr);

    virtual QJsonObject serializeModel() const override;

private:
    QString m_allowedDirections;
};

class PolygonFigure: public PolyFigure
{
public:
    PolygonFigure(QObject* parent = nullptr);
};

class ObjectSizeConstraints: public ObjectValueItem
{
    Q_OBJECT

public:
    ObjectSizeConstraints(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
