#include "voice_spectrum_item.h"

#include <cmath>

#include <QtCore/QElapsedTimer>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGFlatColorMaterial>

#include <utils/media/voice_spectrum_analyzer.h>
#include <nx/utils/raii_guard.h>

using VisualizerData = decltype(QnSpectrumData::data);

namespace {

VisualizerData generateEmptyData(qint64 elapsedMs)
{
    /* Making slider move forth and back.. */
    const int size = QnVoiceSpectrumAnalyzer::bandsCount();
    const int maxIdx = size * 2 - 1;

    VisualizerData result(QnVoiceSpectrumAnalyzer::bandsCount(), 0.0);
    int idx = qRound(16.0 * elapsedMs / 1000) % maxIdx;
    if (idx >= size)
        idx = maxIdx - idx;

    const bool isValidIndex = idx >= 0 && idx < result.size();
    NX_ASSERT(isValidIndex, Q_FUNC_INFO, "Invalid timeStep value");
    if (isValidIndex)
        result[idx] = 0.2;
    return result;
}

void normalizeData(VisualizerData& source)
{
    static constexpr qreal kNormalizerSilenceValue = 0.1;
    static constexpr qreal kNormalizerIncreaseValue = 0.9;

    if (source.isEmpty())
        return;

    const auto maxValue = *std::max_element(source.cbegin(), source.cend());

    // Do not normalize if silence.
    if (maxValue < kNormalizerSilenceValue)
        return;

    // Do not normalize if there is bigger value, so normalizing will always only increase values.
    if (maxValue > kNormalizerIncreaseValue)
        return;

    const auto factor = kNormalizerIncreaseValue / maxValue;
    for (auto &element: source)
        element *= factor;
}

VisualizerData animateData(
    const VisualizerData& prev,
    const VisualizerData& next,
    qint64 timeStepMs)
{
    static constexpr qreal kVisualizerAnimationUpSpeed = 5.0;
    static constexpr qreal kVisualizerAnimationDownSpeed = 1.0;

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

int toRange(int start, int finish, int value)
{
    return std::min(std::max(start, value), finish);
}
VisualizerData interpolate(const VisualizerData& source, int size)
{
    if (source.isEmpty() || !size)
        return VisualizerData();

    VisualizerData data(size, 0.0);
    const double increment = (double) source.count() / double(size);
    for (int index = 0; index != size; ++index)
    {
        const double rawIndexPart = increment * index;
        const double floorIndexPart = rawIndexPart - std::floor(rawIndexPart);
        const double ceilIndexPart = 1.0 - floorIndexPart;
        const int floorIndex = toRange(0, source.count() - 1, std::floor(rawIndexPart));
        const int ceilIndex = toRange(0, source.count() - 1, std::ceil(rawIndexPart));
        data[index] = source[floorIndex] * floorIndexPart + source[ceilIndex] * ceilIndexPart;
    }
    return data;
}

} // namespace

namespace nx {
namespace client {
namespace mobile {

class VoiceSpectrumItem::VisualizerDataGenerator
{
public:
    VisualizerDataGenerator();

    bool setWidth(int value);

    bool setHeight(int value);

    int linesCount() const;

    int lineWidth() const;
    bool setLineWidth(int value);

    int lineSpacing() const;
    bool setLineSpacing(int value);

    VisualizerData getUpdatedData();

private:
    QElapsedTimer m_timer;
    qint64 m_lastTimestampMs = 0;
    VisualizerData m_lastData;

    int m_width = 0;
    int m_height = 0;
    int m_lineWidth = 2;
    int m_lineSpacing = 1;
};

VoiceSpectrumItem::VisualizerDataGenerator::VisualizerDataGenerator()
{
    m_timer.start();
}

bool VoiceSpectrumItem::VisualizerDataGenerator::setWidth(int value)
{
    if (m_width == value)
        return false;

    m_width = value;
    return true;
}

bool VoiceSpectrumItem::VisualizerDataGenerator::setHeight(int value)
{
    if (m_height == value)
        return false;

    m_height = value;
    return true;
}

int VoiceSpectrumItem::VisualizerDataGenerator::linesCount() const
{
    NX_EXPECT(m_lineWidth + m_lineSpacing);
    return m_width / (m_lineWidth + m_lineSpacing);
}

int VoiceSpectrumItem::VisualizerDataGenerator::lineWidth() const
{
    return m_lineWidth;
}

bool VoiceSpectrumItem::VisualizerDataGenerator::setLineWidth(int value)
{
    if (m_lineWidth == value)
        return false;

    m_lineWidth = value;
    return true;
}

int VoiceSpectrumItem::VisualizerDataGenerator::lineSpacing() const
{
    return m_lineSpacing;
}

bool VoiceSpectrumItem::VisualizerDataGenerator::setLineSpacing(int value)
{
    if (m_lineSpacing == value)
        return false;

    m_lineSpacing = value;
    return true;
}

VisualizerData VoiceSpectrumItem::VisualizerDataGenerator::getUpdatedData()
{
    const auto currentTimestampMs = m_timer.elapsed();
    const auto timeDifferenceMs = currentTimestampMs - m_lastTimestampMs;
    m_lastTimestampMs = currentTimestampMs;

    auto currentData = QnVoiceSpectrumAnalyzer::instance()->getSpectrumData().data;
    if (currentData.isEmpty())
        return generateEmptyData(currentTimestampMs);

    normalizeData(currentData);
    m_lastData = interpolate(animateData(m_lastData, currentData, timeDifferenceMs), linesCount());
    return m_lastData;
}

//

VoiceSpectrumItem::VoiceSpectrumItem(QQuickItem* parent):
    base_type(parent),
    m_generator(new VisualizerDataGenerator()),
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
    connect(this, &VoiceSpectrumItem::lineWidthChanged,
        this, &VoiceSpectrumItem::updateNodeGeometry);
    connect(this, &VoiceSpectrumItem::lineSpacingChanged,
        this, &VoiceSpectrumItem::updateNodeGeometry);
}

VoiceSpectrumItem::~VoiceSpectrumItem()
{
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

int VoiceSpectrumItem::lineWidth() const
{
    return m_generator->lineWidth();
}

void VoiceSpectrumItem::setLineWidth(int value)
{
    if (m_generator->setLineWidth(value))
        emit lineWidthChanged();
}

int VoiceSpectrumItem::lineSpacing() const
{
    return m_generator->lineSpacing();
}

void VoiceSpectrumItem::setLineSpacing(int value)
{
    if (m_generator->setLineSpacing(value))
        emit lineSpacingChanged();
}

void VoiceSpectrumItem::clearGeometryData()
{
    const int pointsCount = m_geometry ? m_geometry->vertexCount() : 0;
    if (!pointsCount)
        return;

    const auto& data = m_geometry->vertexDataAsPoint2D();
    for (int i = 0; i != pointsCount; ++i)
        data[i].set(0, 0);
}

void VoiceSpectrumItem::updateNodeGeometry()
{
    const auto node = getNode();
    const auto currentLineWidth = m_generator->lineWidth();
    if (m_generator->setWidth(width()))
    {
        const auto linesCount = m_generator->linesCount();
        m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),  linesCount * 2);
        m_geometry->setDrawingMode(GL_LINES);
        m_geometry->setLineWidth(currentLineWidth);
        clearGeometryData();

        node->setGeometry(m_geometry);
    }

    const int itemHeight = height();
    if (!itemHeight)
        return;

    const auto updateGuard = QnRaiiGuard::createDestructible(
        [this, node]()
        {
            node->markDirty(QSGNode::DirtyGeometry);
            update();
        });

    m_generator->setHeight(height());

    const auto data = m_generator->getUpdatedData();
    if (m_geometry->vertexCount() != data.count() * 2)
    {
        clearGeometryData();
        return;
    }

    const auto currentLineSpacing = m_generator->lineSpacing();
    const auto lineBlockWidth = currentLineWidth + currentLineSpacing;
    auto points = m_geometry->vertexDataAsPoint2D();
    for (int i = 0; i < data.count(); ++i)
    {
        const int x = lineBlockWidth * i;
        const int lineHeight = qMax<int>(2, data[i] * itemHeight);
        const int startY = (itemHeight - lineHeight) / 2;
        const int finishY = startY + lineHeight;
        const int firstIndex = i * 2;
        const int secondIndex = firstIndex + 1;
        points[firstIndex].set(x, startY);
        points[secondIndex].set(x, finishY);
    }
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

