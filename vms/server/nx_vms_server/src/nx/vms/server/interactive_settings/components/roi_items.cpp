#include "roi_items.h"

#include <QtCore/QJsonDocument>

namespace nx::vms::server::interactive_settings::components {

BaseFigure::BaseFigure(const QString& figureType, QObject* parent):
    ValueItem(figureType, parent)
{
}

/*static*/ QJsonObject BaseFigure::mergeFigures(
    const QJsonObject& currentFigureValue, const QJsonObject& newFigureValue)
{
    QJsonObject result = currentFigureValue;
    for (auto it = newFigureValue.begin(); it != newFigureValue.end(); ++it)
    {
        const QString propertyKey = it.key();

        const static QStringList kNullableFields = { "figure", "points" };
        if ((it->isNull() || it->isUndefined()) && !kNullableFields.contains(propertyKey))
            continue;

        if (it->isObject())
        {
            result[propertyKey] =
                mergeFigures(result[propertyKey].toObject(), it.value().toObject());
        }
        else
        {
            result[propertyKey] = it.value();
        }
    }

    return result;
}

void BaseFigure::setValue(const QVariant& value)
{
    QJsonObject valueJsonObject;
    if (value.canConvert<std::nullptr_t>()) //< Must be handled first.
    {
        m_value.setValue(nullptr);
        return;
    }
    else if (value.canConvert<QString>())
    {
        QJsonDocument jsonDocument = QJsonDocument::fromJson(value.toString().toUtf8());
        if (jsonDocument.isObject())
            valueJsonObject = jsonDocument.object();
        else
            return;
    }
    else if (value.canConvert<QVariantMap>())
    {
        valueJsonObject = QJsonValue::fromVariant(value).toObject();
    }
    else
    {
        return;
    }

    m_value.setValue(mergeFigures(QJsonValue::fromVariant(m_value).toObject(), valueJsonObject));
}

void PolyFigure::setMinPoints(int minPoints)
{
    if (m_minPoints == minPoints)
        return;

    m_minPoints = minPoints;
    emit minPointsChanged();
}

void PolyFigure::setMaxPoints(int maxPoints)
{
    if (m_maxPoints == maxPoints)
        return;

    m_maxPoints = maxPoints;
    emit maxPointsChanged();
}

QJsonObject PolyFigure::serialize() const
{
    auto result = base_type::serialize();
    result[QStringLiteral("minPoints")] = minPoints();
    result[QStringLiteral("maxPoints")] = maxPoints();
    return result;
}

LineFigure::LineFigure(QObject* parent):
    PolyFigure(QStringLiteral("LineFigure"), parent)
{
}

QJsonObject LineFigure::serialize() const
{
    auto result = PolyFigure::serialize();
    result[QStringLiteral("allowedDirections")] = m_allowedDirections;
    return result;
}

PolygonFigure::PolygonFigure(QObject* parent):
    PolyFigure(QStringLiteral("PolygonFigure"), parent)
{
}

BoxFigure::BoxFigure(QObject* parent):
    BaseFigure(QStringLiteral("BoxFigure"), parent)
{
}

ObjectSizeConstraints::ObjectSizeConstraints(QObject* parent):
    ValueItem(QStringLiteral("ObjectSizeConstraints"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
