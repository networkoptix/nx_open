// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fisheye_calibrator.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

#include <QtCore/QMetaEnum>

#include <nx/utils/math/math.h>

namespace {

static constexpr int kBufferAlignment = 32;

struct Distance
{
    qint64 distance = std::numeric_limits<qint64>::max();
    QPointF pos;
    Qt::Corner corner = Qt::TopLeftCorner;

    bool operator< (const Distance& other) const
    {
        return distance < other.distance;
    }
};

inline void pixSort(quint8& a, quint8& b)
{
    if (a > b)
        std::swap(a, b);
}

quint8 opt_med5(quint8* p)
{
    pixSort(p[0], p[1]);
    pixSort(p[3], p[4]);
    pixSort(p[0], p[3]);
    pixSort(p[1], p[4]);
    pixSort(p[1], p[2]);
    pixSort(p[2], p[3]);
    pixSort(p[1], p[2]);
    return(p[2]);
}

quint8 opt_med9(quint8* p)
{
    pixSort(p[1], p[2]);
    pixSort(p[4], p[5]);
    pixSort(p[7], p[8]);
    pixSort(p[0], p[1]);
    pixSort(p[3], p[4]);
    pixSort(p[6], p[7]);
    pixSort(p[1], p[2]);
    pixSort(p[4], p[5]);
    pixSort(p[7], p[8]);
    pixSort(p[0], p[3]);
    pixSort(p[5], p[8]);
    pixSort(p[4], p[7]);
    pixSort(p[3], p[6]);
    pixSort(p[1], p[4]);
    pixSort(p[2], p[5]);
    pixSort(p[4], p[7]);
    pixSort(p[4], p[2]);
    pixSort(p[6], p[4]);
    pixSort(p[4], p[2]);
    return(p[4]);
}

} // namespace

namespace nx::vms::client::desktop {

struct FisheyeCalibrator::Private
{
    FisheyeCalibrator* const q;

    QImage analyzedFrame;

    QPointF center;
    qreal stretch = 1.0;
    qreal radius = 0.0;

    std::vector<quint8> filteredFrame;
    int width = 0;
    int height = 0;

    int findPixel(int y, bool reverse) const;
    int calculateLuminanceThreshold(QImage frame) const;
    QPointF calculateEllipseRadii(const QPointF& pixCenter, qreal pixRadius) const;

    Result computeParameters();
};

std::ostream& operator<<(std::ostream& os, FisheyeCalibrator::Result value)
{
    return os << QMetaEnum::fromType<FisheyeCalibrator::Result>().valueToKey(int(value));
}

// ------------------------------------------------------------------------------------------------
// FisheyeCalibrator

FisheyeCalibrator::FisheyeCalibrator():
    d(new Private{this})
{
    qRegisterMetaType<Result>();

    connect(this, &QThread::started, this, &FisheyeCalibrator::runningChanged);
    connect(this, &QThread::finished, this, &FisheyeCalibrator::runningChanged);
}

FisheyeCalibrator::~FisheyeCalibrator()
{
    if (isRunning())
        stop();
}

QPointF FisheyeCalibrator::center() const
{
    return d->center;
}

qreal FisheyeCalibrator::stretch() const
{
    return d->stretch;
}

qreal FisheyeCalibrator::radius() const
{
    return d->radius;
}

void FisheyeCalibrator::setCenter(const QPointF& value)
{
    const QPointF newCenter(qBound(0.0, value.x(), 1.0), qBound(0.0, value.y(), 1.0));
    if (qFuzzyEquals(d->center, newCenter))
        return;

    d->center = newCenter;
    emit centerChanged(d->center);
}

void FisheyeCalibrator::setStretch(const qreal& value)
{
    if (qFuzzyEquals(d->stretch, value))
        return;

    d->stretch = value;
    emit stretchChanged(d->stretch);
}

void FisheyeCalibrator::setRadius(qreal value)
{
    const qreal newRadius = qBound(0.0, value, 0.75);
    if (qFuzzyEquals(d->radius, newRadius))
        return;

    d->radius = newRadius;
    emit radiusChanged(d->radius);
}

void FisheyeCalibrator::analyzeFrameAsync(QImage frame)
{
    if (isRunning())
        stop();

    d->analyzedFrame = frame;
    start();
}

FisheyeCalibrator::Result FisheyeCalibrator::analyzeFrame(QImage frame)
{
    if (frame.width() == 0 || frame.height() == 0)
        return Result::errorInvalidInput;

    frame = frame.scaled(frame.width() / 2, frame.height() / 2); //< Additional downscaling.
    frame.convertTo(QImage::Format_Grayscale8);

    const int lumaThreshold = d->calculateLuminanceThreshold(frame);

    if (lumaThreshold == -1)
        return Result::errorTooLowLight;

    const quint8* curPtr = (const quint8*)frame.bits();
    if (!curPtr)
        return Result::errorInvalidInput;

    d->filteredFrame.resize(frame.width() * frame.height());
    d->width = frame.width();
    d->height = frame.height();

    auto destination = d->filteredFrame.begin();
    const int srcYStep = frame.bytesPerLine();

    std::array<quint8, 9> data{};

    std::fill_n(destination, d->width, 0);
    destination += d->width;
    curPtr += srcYStep;

    for (int y = 1; y < d->height - 1; ++y)
    {
        if (needToStop())
            return Result::errorInterrupted;

        *destination++ = 0;
        curPtr++;

        for (int x = 1; x < d->width - 1; ++x)
        {
            data[0] = curPtr[srcYStep - 1];
            data[1] = curPtr[-srcYStep];
            data[2] = curPtr[-srcYStep + 1];
            data[3] = curPtr[-1];
            data[4] = curPtr[0];
            data[5] = curPtr[1];
            data[6] = curPtr[-srcYStep - 1];
            data[7] = curPtr[srcYStep];
            data[8] = curPtr[srcYStep + 1];

            *destination++ = opt_med9(data.data()) > lumaThreshold ? 240 : 0;
            curPtr++;
        }

        *destination++ = 0;
        curPtr += srcYStep - d->width + 1;
    }

    std::fill_n(destination, d->width, 0);

    return d->computeParameters();
}

void FisheyeCalibrator::run()
{
    const auto result = analyzeFrame(d->analyzedFrame);
    emit finished(result);
}

// ------------------------------------------------------------------------------------------------
// FisheyeCalibrator::Private

int FisheyeCalibrator::Private::findPixel(int y, bool reverse) const
{
    const int x = reverse ? (width - 1) : 0;
    const int sign = reverse ? -1 : 1;

    const auto line = filteredFrame.cbegin() + y * width;
    const auto begin = line + x;
    const auto end = begin + sign * width;

    for (auto it = begin; it != end; it += sign)
    {
        if (*it > 0)
            return it - line;
    }

    return -1;
}

QPointF FisheyeCalibrator::Private::calculateEllipseRadii(
    const QPointF& pixCenter, qreal pixRadius) const
{
    static const std::array<qreal, 2> kOffsets{{0.33, 0.66}};

    QPointF p1, p2;
    for (int i = 0; i < 2; ++i)
    {
        const qreal offset = pixRadius * kOffsets[i];
        const int yMax = qMin(pixCenter.y() + offset, height - 1);
        const int yMin = qMax(pixCenter.y() - offset, 0);

        const int x1 = findPixel(yMax, /*reverse*/ true);
        const int x2 = findPixel(yMax, /*reverse*/ false);
        const int x3 = findPixel(yMin, /*reverse*/ true);
        const int x4 = findPixel(yMin, /*reverse*/ false);

        std::array<Distance, 4> distances{{
            Distance{(qint64) qAbs(pixCenter.x() - x1), QPointF(x1, yMax), Qt::BottomRightCorner},
            Distance{(qint64) qAbs(pixCenter.x() - x2), QPointF(x2, yMax), Qt::BottomLeftCorner},
            Distance{(qint64) qAbs(pixCenter.x() - x3), QPointF(x3, yMin), Qt::TopRightCorner},
            Distance{(qint64) qAbs(pixCenter.x() - x4), QPointF(x4, yMin), Qt::TopLeftCorner}}};

        std::sort(distances.begin(), distances.end());
        if (i == 0)
            p1 = QPointF(distances[3].pos);
        else
            p2 = QPointF(distances[3].pos);
    }

    p1 -= pixCenter;
    p2 -= pixCenter;

    const qreal c = p2.x() * p2.x() * p1.y() * p1.y() - p1.x() * p1.x() * p2.y() * p2.y();
    const qreal k = p2.x() * p2.x() - p1.x() * p1.x();

    if (qFuzzyIsNull(k))
        return {0, 0};

    const qreal b = sqrt(qAbs(c / k));

    if (qFuzzyEquals(b, p1.y()))
        return {0, 0};

    const qreal a = sqrt(qAbs(p1.x() * p1.x() * b * b / (b * b - p1.y() * p1.y())));
    return {a, b};
}

FisheyeCalibrator::Result FisheyeCalibrator::Private::computeParameters()
{
    std::array<Distance, 4> distances;

    const int rightEdge = width - 1;
    for (int y = 0; y < height; ++y)
    {
        if (q->needToStop())
            return Result::errorInterrupted;

        const int xLeft = findPixel(y, false);
        if (xLeft == -1)
            continue;

        auto distance = xLeft * xLeft + y * y;
        if (distance < distances[Qt::TopLeftCorner].distance)
        {
            distances[Qt::TopLeftCorner].distance = distance;
            distances[Qt::TopLeftCorner].pos = QPointF(xLeft, y);
        }

        const int xRight = findPixel(y, true);
        if (xRight == -1)
            continue;

        distance = (rightEdge - xRight) * (rightEdge - xRight) + y * y;
        if (distance < distances[Qt::TopRightCorner].distance)
        {
            distances[Qt::TopRightCorner].distance = distance;
            distances[Qt::TopRightCorner].pos = QPointF(xRight, y);
        }
    }

    const int bottomEdge = height - 1;
    for (int y = height - 1; y >= 0; --y)
    {
        if (q->needToStop())
            return Result::errorInterrupted;

        const int xLeft = findPixel(y, false);
        if (xLeft == -1)
            continue;

        auto distance = xLeft * xLeft + (bottomEdge - y) * (bottomEdge - y);
        if (distance < distances[Qt::BottomLeftCorner].distance)
        {
            distances[Qt::BottomLeftCorner].distance = distance;
            distances[Qt::BottomLeftCorner].pos = QPoint(xLeft, y);
        }

        const int xRight = findPixel(y, true);
        if (xRight == -1)
            continue;

        distance = (rightEdge - xRight) * (rightEdge - xRight) + (bottomEdge - y) * (bottomEdge - y);
        if (distance < distances[Qt::BottomRightCorner].distance)
        {
            distances[Qt::BottomRightCorner].distance = distance;
            distances[Qt::BottomRightCorner].pos = QPoint(xRight, y);
        }
    }

    std::sort(distances.begin(), distances.end());
    if (distances[3].distance == std::numeric_limits<qint64>::max() || distances[0].distance == 0)
        return Result::errorNotFisheyeImage; //< Not found.

    // Remove the farthest point.
    const int leftDistance = distances[1].distance - distances[0].distance;
    const int rightDistance = distances[3].distance - distances[2].distance;
    int startIdx = 0;
    if (leftDistance > rightDistance)
        startIdx = 1;

    QPointF a1(distances[startIdx].pos);
    QPointF a2(distances[startIdx + 1].pos);
    QPointF a3(distances[startIdx + 2].pos);

    if (a1.x() == a2.x())
        qSwap(a2, a3);
    else if (a2.x() == a3.x())
        qSwap(a1, a3);
    else if (a1.y() == a2.y())
        qSwap(a1, a3);

    const qreal ma = (a2.y() - a1.y()) / (a2.x() - a1.x());
    const qreal mb = (a3.y() - a2.y()) / (a3.x() - a2.x());

    if ((ma == 0 && mb == 0) || ma > 1e9 || mb > 1e9 || ma < -1e9 || mb < -1e9)
        return Result::errorNotFisheyeImage;

    const qreal centerX = (ma * mb * (a1.y() - a3.y()) + mb * (a1.x() + a2.x())
        - ma * (a2.x() + a3.x())) / (2 * (mb - ma));

    const qreal centerY = -(1.0 / ma) * (centerX - (a1.x() + a2.x()) / 2.0) + (a1.y() + a2.y())
        / 2.0;

    const qreal dx = centerX - a1.x();
    const qreal dy = centerY - a1.y();
    const qreal r = sqrt(dx * dx + dy * dy) + 0.5;

    const auto radii = calculateEllipseRadii({centerX, centerY}, r / width * height);
    if (qFuzzyIsNull(radii.x()) || qFuzzyIsNull(radii.y()))
        return Result::errorNotFisheyeImage; //< Not found.

    qreal stretch = radii.x() / radii.y();
    qreal radius = radii.x() / width;

    // If computed stretch is near 1, consider it 1.
    if (qAbs(stretch - 1.0) < 0.1)
    {
        stretch = 1.0;
        radius = r / width;
    }

    q->setCenter({centerX / width, centerY / height});
    q->setRadius(radius);
    q->setStretch(stretch);

    return Result::ok;
}

int FisheyeCalibrator::Private::calculateLuminanceThreshold(QImage frame) const
{
    static constexpr int kMaxLuminanceThreshold = 64;
    static constexpr int kMinLuminanceThreshold = 32;
    static constexpr int kDetectBorderDelta = 32;

    QVector<int> borders;
    for (int y = 0; y < frame.height(); ++y)
    {
        const quint8* line = frame.bits() + y * frame.bytesPerLine();
        for (int x = 0; x < frame.width() - 2; ++x)
        {
            if (line[x] >= kMaxLuminanceThreshold)
                break;

            if (line[x + 1] - line[x] >= kDetectBorderDelta)
            {
                borders << line[x + 1] - 1;
                break;
            }

            if (line[x + 2] - line[x] >= kDetectBorderDelta)
            {
                borders << line[x + 2] - 1;
                break;
            }
        }
    }

    if (borders.isEmpty())
        return -1;

    std::sort(borders.begin(), borders.end());
    const int result = borders[borders.size() / 2];

    return qBound(kMinLuminanceThreshold, result, kMaxLuminanceThreshold);
}

} // namespace nx::vms::client::desktop
