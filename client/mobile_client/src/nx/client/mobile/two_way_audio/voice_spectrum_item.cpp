#include "voice_spectrum_item.h"

#include <cmath>

#include <QtQml/QtQml>

#include <QtCore/QElapsedTimer>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGFlatColorMaterial>

#include <utils/media/voice_spectrum_analyzer.h>
#include <nx/utils/scope_guard.h>

using VisualizerData = decltype(QnSpectrumData::data);

namespace {

void clearGeometryData(QSGGeometry* geometry)
{
    const auto vertexCount = geometry->vertexCount();
    if (vertexCount)
    {
        const auto data = geometry->vertexDataAsPoint2D();
        memset(data, 0, sizeof(QSGGeometry::Point2D) * vertexCount);
    }
}

VisualizerData generateEmptyData(qint64 elapsedMs)
{
    // Making slider move forth and back.
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
    for (auto& element: source)
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

// Returns array with new size filled by source data.
VisualizerData interpolate(const VisualizerData& source, int size)
{
    if (source.isEmpty() || !size)
        return VisualizerData();

    VisualizerData data(size, 0.0);
    const double increment = (double) source.count() / size;
    for (int index = 0; index != size; ++index)
    {
        const double rawIndexPart = increment * index;
        const double floorIndexPart = rawIndexPart - std::floor(rawIndexPart);
        const double ceilIndexPart = 1.0 - floorIndexPart;
        const int floorIndex = qBound<int>(0, std::floor(rawIndexPart), source.count() - 1);
        const int ceilIndex = qBound<int>(0, std::ceil(rawIndexPart), source.count() - 1);
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
    const int totalLineWidth = m_lineWidth + m_lineSpacing;
    if (totalLineWidth > 0)
        return m_width / totalLineWidth;

    NX_ASSERT(false, "Wrong line paramaters");
    return 0;
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

    auto currentData = interpolate(QnVoiceSpectrumAnalyzer::instance()->getSpectrumData().data,
        linesCount());
    if (currentData.isEmpty())
        return interpolate(generateEmptyData(currentTimestampMs), linesCount());

    normalizeData(currentData);
    m_lastData = interpolate(animateData(m_lastData, currentData, timeDifferenceMs), linesCount());
    return m_lastData;
}

//-------------------------------------------------------------------------------------------------

VoiceSpectrumItem::VoiceSpectrumItem(QQuickItem* parent):
    base_type(parent),
    m_generator(new VisualizerDataGenerator())
{
    setFlag(QQuickItem::ItemHasContents);

    connect(this, &VoiceSpectrumItem::colorChanged, this,
        [this]()
        {
            m_updateMaterial = true;
            update();
        });

    connect(this, &VoiceSpectrumItem::widthChanged,
        this, &VoiceSpectrumItem::update);
    connect(this, &VoiceSpectrumItem::heightChanged,
        this, &VoiceSpectrumItem::update);
    connect(this, &VoiceSpectrumItem::lineWidthChanged,
        this, &VoiceSpectrumItem::update);
    connect(this, &VoiceSpectrumItem::lineSpacingChanged,
        this, &VoiceSpectrumItem::update);
}

VoiceSpectrumItem::~VoiceSpectrumItem()
{
}

void VoiceSpectrumItem::registerQmlType()
{
    qmlRegisterType<VoiceSpectrumItem>("nx.client.mobile", 1, 0, "VoiceSpectrumItem");
}

QSGNode* VoiceSpectrumItem::updatePaintNode(
    QSGNode* node,
    UpdatePaintNodeData* /*updatePaintNodeData*/)
{
    if (!node)
        return createNode();

    const auto geometryNode = static_cast<QSGGeometryNode*>(node);
    updateNodeGeometry(geometryNode);
    if (m_updateMaterial)
    {
        updateNodeMaterial(geometryNode);
        m_updateMaterial = false;
    }

    return node;
}

QSGGeometryNode* VoiceSpectrumItem::createNode()
{
    QSGGeometryNode* node = new QSGGeometryNode();
    const auto material = new QSGFlatColorMaterial();
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setFlag(QSGNode::OwnsMaterial);

    updateNodeMaterial(node);
    updateNodeGeometry(node);

    return node;
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

void VoiceSpectrumItem::updateNodeGeometry(QSGGeometryNode* node)
{
    const auto currentLineWidth = m_generator->lineWidth();
    auto geometry = node->geometry();
    if (!geometry || m_generator->setWidth(width()))
    {
        const auto linesCount = m_generator->linesCount();
        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),  linesCount * 6);
        geometry->setDrawingMode(GL_TRIANGLES);
        geometry->setLineWidth(currentLineWidth);
        clearGeometryData(geometry);

        node->setGeometry(geometry);
    }

    const int itemHeight = height();
    if (!itemHeight)
        return;

    const auto updateGuard = nx::utils::makeScopeGuard(
        [this, node]()
        {
            node->markDirty(QSGNode::DirtyGeometry);
            update(); // We expect infinite update cycle in this item.
        });

    m_generator->setHeight(height());

    const auto data = m_generator->getUpdatedData();
    if (geometry->vertexCount() != data.count() * 6)
    {
        clearGeometryData(geometry);
        return;
    }

    const auto currentLineSpacing = m_generator->lineSpacing();
    const auto lineBlockWidth = currentLineWidth + currentLineSpacing;
    auto points = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < data.count(); ++i)
    {
        const float lineHeight = qMax<float>(2, data[i] * itemHeight);

        const float left = lineBlockWidth * i;
        const float top = (itemHeight - lineHeight) / 2;
        const float bottom = top + lineHeight;
        const float right = left + currentLineWidth;

        points[0].set(left, top);
        points[1].set(left, bottom);
        points[2].set(right, bottom);

        points[3].set(left, top);
        points[4].set(right, bottom);
        points[5].set(right, top);

        points += 6;
    }
}

void VoiceSpectrumItem::updateNodeMaterial(QSGGeometryNode* node)
{
    const auto colorMaterial = static_cast<QSGFlatColorMaterial*>(node->material());
    colorMaterial->setColor(color());
    node->markDirty(QSGNode::DirtyMaterial);
}

} // namespace mobile
} // namespace client
} // namespace nx

