#include "voice_spectrum_item.h"

#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGFlatColorMaterial>

#include <utils/media/voice_spectrum_analyzer.h>

using VisualizerData = decltype(QnSpectrumData::data);

namespace {
/*
VisualizerData animateVector(
    const VisualizerData& prev,
    const VisualizerData& next,
    qint64 timeStepMs)
{
    if (prev.size() != next.size())
        return next;

    const qreal maxUpChange = qBound(0.0, kVisualizerAnimationUpSpeed * timeStepMs / 1000, 1.0);
    const qreal maxDownChange = qBound(0.0, kVisualizerAnimationDownSpeed * timeStepMs / 1000, 1.0);

    VisualizerData result(prev.size());
    for (int i = 0; i < prev.size(); ++i)
    {
        auto current = prev[i];
        auto target = next[i];
        auto change = target - current;
        if (change > 0)
            change = qMin(change, maxUpChange);
        else
            change = qMax(change, -maxDownChange);
        result[i] = qBound(0.0, current + change, 1.0);
    }
    return result;
}
*/
} // namespace

namespace nx {
namespace client {
namespace mobile {

VoiceSpectrumItem::VoiceSpectrumItem(QQuickItem* parent):
    base_type(parent),
    m_material(new QSGFlatColorMaterial())
{
    setFlag(QQuickItem::ItemHasContents);

    connect(this, &VoiceSpectrumItem::colorChanged,
        this, &VoiceSpectrumItem::updateNodeMaterial);

    connect(this, &VoiceSpectrumItem::widthChanged,
        this, &VoiceSpectrumItem::updateNodeGeometry);
    connect(this, &VoiceSpectrumItem::heightChanged,
        this, &VoiceSpectrumItem::updateNodeGeometry);
    connect(this, &VoiceSpectrumItem::geometryChanged,
        this, &VoiceSpectrumItem::updateNodeGeometry);
}

QSGNode* VoiceSpectrumItem::updatePaintNode(
    QSGNode* node,
    UpdatePaintNodeData* updatePaintNodeData)
{
    const auto geometryNode = node
        ? static_cast<QSGGeometryNode*>(node)
        : getNode();

    updateNodeGeometry();
    return geometryNode;
}

QSGGeometryNode* VoiceSpectrumItem::getNode()
{
    if (m_node)
        return m_node;

    m_node = new QSGGeometryNode();
    m_node->setMaterial(m_material);
    m_node->setFlag(QSGNode::OwnsGeometry);
    m_node->setFlag(QSGNode::OwnsMaterial);

    updateNodeMaterial();
    updateNodeGeometry();

    return m_node;
}

int VoiceSpectrumItem::elementsCount() const
{
    return m_elementsCount;
}

void VoiceSpectrumItem::setElementsCount(int value)
{
    if (m_elementsCount == value)
        return;

    m_elementsCount = value;
    emit elementsCountChanged();
}

QColor VoiceSpectrumItem::color() const
{
    return m_color;
}

void VoiceSpectrumItem::setColor(const QColor& color)
{
    if (m_color == color)
        return;

    m_color = color;
    emit colorChanged();
}

void VoiceSpectrumItem::updateNodeGeometry()
{
    const auto currentWidth = width();
    if (m_lastWidth != currentWidth)
    {
        const auto linesCount = currentWidth / 3; // 2px for line and 1 between.
        m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), linesCount * 2);
        m_geometry->setDrawingMode(GL_LINES);
        m_geometry->setLineWidth(2);

        getNode()->setGeometry(m_geometry);
    }

    const int itemHeight = height();
    if (!itemHeight)
        return;

    auto points = m_geometry->vertexDataAsPoint2D();
    for (int i = 0; i < m_geometry->vertexCount() / 2; ++i)
    {
        const int x = i * 3;
        const int lineHeight = rand() % itemHeight;
        const int startY = (itemHeight - lineHeight) / 2;
        const int finishY = startY + lineHeight;
        const int firstIndex = i * 2;
        const int secondIndex = firstIndex + 1;
        points[firstIndex].set(x, startY);
        points[secondIndex].set(x, finishY);
    }

    m_lastWidth = currentWidth;
    getNode()->markDirty(QSGNode::DirtyGeometry);
    update();
}

void VoiceSpectrumItem::updateNodeMaterial()
{
    const auto node = getNode();
    if (!node->material())
        node->setMaterial(m_material);

    m_material->setColor(color());
    getNode()->markDirty(QSGNode::DirtyMaterial);
    update();
}


} // namespace mobile
} // namespace client
} // namespace nx

