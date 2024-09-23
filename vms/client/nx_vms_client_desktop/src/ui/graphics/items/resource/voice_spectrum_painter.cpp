// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "voice_spectrum_painter.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <nx/utils/log/assert.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/media/voice_spectrum_analyzer.h>

namespace nx::vms::client::desktop {

VoiceSpectrumPainter::VoiceSpectrumPainter() = default;
VoiceSpectrumPainter::~VoiceSpectrumPainter() = default;

void VoiceSpectrumPainter::update(qint64 timeMs, const Data& data)
{
    const auto timeStepMs = timeMs - m_oldTimeMs;
    m_oldTimeMs = timeMs;

    if (data.isEmpty())
    {
        m_data = generateEmptyData(timeMs, QnVoiceSpectrumAnalyzer::bandsCount());
    }
    else
    {
        Data normalized = data;
        normalizeData(normalized);

        m_data = animateData(m_data, normalized, timeStepMs);
    }
}

void VoiceSpectrumPainter::paint(QPainter* painter, const QRectF& rect)
{
    if (m_data.isEmpty())
        return;

    const qreal lineWidth = qRound(qMax(options.visualizerLineWidth,
        (rect.width() / m_data.size()) - options.visualizerLineOffset));

    const qreal midY = rect.center().y();
    const qreal maxHeight = rect.height() * options.maxVisualizerHeightCoeff;

    QPainterPath path;
    for (int i = 0; i < m_data.size(); ++i)
    {
        const qreal lineHeight = qRound(qMax(maxHeight * m_data[i], options.visualizerLineOffset * 2));
        path.addRect(qRound(rect.left() + i * (lineWidth + options.visualizerLineOffset)),
            qRound(midY - (lineHeight / 2)), lineWidth, lineHeight);
    }

    paintSharp(painter,
        [&](QPainter* painter) { painter->fillPath(path, options.color); });
}

void VoiceSpectrumPainter::reset()
{
    m_data = Data();
}

void VoiceSpectrumPainter::normalizeData(Data& source)
{
    if (source.isEmpty())
        return;

    const auto max = std::max_element(source.cbegin(), source.cend());

    // Do not normalize if silence.
    if (*max < options.normalizerSilenceValue)
        return;

    // Do not normalize if there is bigger value, so normalizing will always only increase values.
    if (*max > options.normalizerIncreaseValue)
        return;

    const auto k = options.normalizerIncreaseValue / *max;
    for (auto& e: source)
        e *= k;
}

VoiceSpectrumPainter::Data VoiceSpectrumPainter::animateData(const Data& prev, const Data& next, qint64 timeStepMs)
{
    //NX_ASSERT(next.size() == QnVoiceSpectrumAnalyzer::bandsCount());

    if (prev.size() != next.size())
        return next;

    const qreal maxUpChange = qBound(0.0, options.visualizerAnimationUpSpeed * timeStepMs / 1000, 1.0);
    const qreal maxDownChange = qBound(0.0, options.visualizerAnimationDownSpeed * timeStepMs / 1000, 1.0);

    Data result(prev.size());
    for (int i = 0; i < prev.size(); ++i)
    {
        const auto current = prev[i];
        const auto target = next[i];
        auto change = target - current;
        if (change > 0)
            change = qMin(change, maxUpChange);
        else
            change = qMax(change, -maxDownChange);
        result[i] = qBound(0.0, current + change, 1.0);
    }

    return result;
}

VoiceSpectrumPainter::Data VoiceSpectrumPainter::generateEmptyData(qint64 elapsedMs, int bandsCount)
{
    // Making slider move forth and back...
    const int size = bandsCount;
    const int maxIdx = size * 2 - 1;

    Data result(bandsCount, 0.0);
    int idx = qRound(16.0 * elapsedMs / 1000) % maxIdx;
    if (idx >= size)
        idx = maxIdx - idx;

    const bool isValidIndex = idx >= 0 && idx < result.size();
    NX_ASSERT(isValidIndex, "Invalid timeStep value");
    if (isValidIndex)
        result[idx] = 0.2;

    return result;
}

} // namespace nx::vms::client::desktop
