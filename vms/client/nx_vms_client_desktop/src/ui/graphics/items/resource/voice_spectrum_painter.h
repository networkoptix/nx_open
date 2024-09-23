// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QVector>
#include <QtGui/QColor>

class QPainter;

namespace nx::vms::client::desktop {

struct VoiceSpectrumPainterOptions
{
    /** Max visualizer value change per second (on increasing and decreasing values). */
    qreal visualizerAnimationUpSpeed = 5.0;
    qreal visualizerAnimationDownSpeed = 1.0;

    /** Maximum visualizer height. */
    qreal maxVisualizerHeightCoeff = 0.9;

    /** Values lower than that value will be counted as silence. */
    qreal normalizerSilenceValue = 0.1;

    /** Maximum value, to which all values are normalized. */
    qreal normalizerIncreaseValue = 0.9;

    /** Recommended visualizer line width. */
    qreal visualizerLineWidth = 4.0;

    /** Visualizer offset between lines. */
    qreal visualizerLineOffset = 2.0;

    /** Color to use for audio spectrum painting. */
    QColor color = QColor(255, 255, 255);
};

class VoiceSpectrumPainter
{
public:
    using Data = QVector<double>;

    VoiceSpectrumPainter();
    virtual ~VoiceSpectrumPainter();

    void update(qint64 timeMs, const Data& data);

    void paint(QPainter* painter, const QRectF& rect);

    void reset();

    VoiceSpectrumPainterOptions options;

private:
    void normalizeData(Data& source);
    Data animateData(const Data& prev, const Data& next, qint64 timeStepMs);
    static Data generateEmptyData(qint64 elapsedMs, int bandsCount);

private:
    Data m_data;
    qint64 m_oldTimeMs = 0;
};

} // namespace nx::vms::client::desktop
