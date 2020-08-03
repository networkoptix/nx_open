#include "roi_items.h"

namespace nx::vms::server::interactive_settings::components {

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

void PolyFigure::setUseLabelField(bool useLabelField)
{
    if (m_useLabelField == useLabelField)
        return;

    m_useLabelField = useLabelField;
    emit useLabelFieldChanged();
}

QJsonObject PolyFigure::serializeModel() const
{
    auto result = ObjectValueItem::serializeModel();
    result[QStringLiteral("minPoints")] = minPoints();
    result[QStringLiteral("maxPoints")] = maxPoints();
    result[QStringLiteral("useLabelField")] = useLabelField();
    return result;
}

LineFigure::LineFigure(QObject* parent):
    PolyFigure(QStringLiteral("LineFigure"), parent)
{
}

QJsonObject LineFigure::serializeModel() const
{
    auto result = PolyFigure::serializeModel();
    result[QStringLiteral("allowedDirections")] = m_allowedDirections;
    return result;
}

PolygonFigure::PolygonFigure(QObject* parent):
    PolyFigure(QStringLiteral("PolygonFigure"), parent)
{
}

BoxFigure::BoxFigure(QObject* parent):
    ObjectValueItem(QStringLiteral("BoxFigure"), parent)
{
}

ObjectSizeConstraints::ObjectSizeConstraints(QObject* parent):
    ObjectValueItem(QStringLiteral("ObjectSizeConstraints"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
