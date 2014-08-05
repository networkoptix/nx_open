#include "fisheye_calibrator.h"

enum Corner {
    LeftTop,
    RightTop,
    RightBottom,
    LeftBottom
};

struct Distance
{
    Distance(): distance(INT64_MAX), corner(LeftTop) {}
    Distance(qint64 distance, const QPointF& pos, Corner corner): distance(distance), pos(pos), corner(corner) {}

    qint64 distance;
    QPointF pos;
    Corner corner;

    bool operator< (const Distance& other) const {
        return distance < other.distance;
    }
};



QnFisheyeCalibrator::QnFisheyeCalibrator()
{
    m_filteredFrame = 0;
    m_grayImageBuffer = 0;
    m_width = 0;
    m_height = 0;
    m_center = QPointF(0.0, 0.0);
    m_radius = 0.0;
    m_horizontalStretch = 1.0;
}

QnFisheyeCalibrator::~QnFisheyeCalibrator()
{
    delete []m_filteredFrame;
    qFreeAligned(m_grayImageBuffer);
}

void QnFisheyeCalibrator::setCenter(const QPointF &center) {
    QPointF fixed(qBound(0.0, center.x(), 1.0), qBound(0.0, center.y(), 1.0));

    if (qFuzzyEquals(m_center, fixed))
        return;
    m_center = fixed;
    emit centerChanged(fixed);
}

QPointF QnFisheyeCalibrator::center() const {
    return m_center;
}

void QnFisheyeCalibrator::setHorizontalStretch(const qreal& value)
{
    if (qFuzzyEquals(m_horizontalStretch, value))
        return;
    m_horizontalStretch = value;
    emit stretchChanged(m_horizontalStretch);

}

qreal QnFisheyeCalibrator::horizontalStretch() const
{
    return m_horizontalStretch;
}


void QnFisheyeCalibrator::setRadius(qreal radius) {
    qreal fixed = qBound(0.25, radius, 0.75);
    if (qFuzzyEquals(m_radius, fixed))
        return;
    m_radius = fixed;
    emit radiusChanged(fixed);
}

qreal QnFisheyeCalibrator::radius() const {
    return m_radius;
}

#define PIX_SORT(a,b) { if ((a)>(b)) PIX_SWAP((a),(b)); }
#define PIX_SWAP(a,b) { quint8 temp=(a);(a)=(b);(b)=temp; }

quint8 opt_med5(quint8 * p)
{
    PIX_SORT(p[0],p[1]) ; PIX_SORT(p[3],p[4]) ; PIX_SORT(p[0],p[3]) ;
    PIX_SORT(p[1],p[4]) ; PIX_SORT(p[1],p[2]) ; PIX_SORT(p[2],p[3]) ;
    PIX_SORT(p[1],p[2]) ; return(p[2]) ;
}

quint8 opt_med9(quint8 * p)
{
    PIX_SORT(p[1], p[2]) ; PIX_SORT(p[4], p[5]) ; PIX_SORT(p[7], p[8]) ;
    PIX_SORT(p[0], p[1]) ; PIX_SORT(p[3], p[4]) ; PIX_SORT(p[6], p[7]) ;
    PIX_SORT(p[1], p[2]) ; PIX_SORT(p[4], p[5]) ; PIX_SORT(p[7], p[8]) ;
    PIX_SORT(p[0], p[3]) ; PIX_SORT(p[5], p[8]) ; PIX_SORT(p[4], p[7]) ;
    PIX_SORT(p[3], p[6]) ; PIX_SORT(p[1], p[4]) ; PIX_SORT(p[2], p[5]) ;
    PIX_SORT(p[4], p[7]) ; PIX_SORT(p[4], p[2]) ; PIX_SORT(p[6], p[4]) ;
    PIX_SORT(p[4], p[2]) ; return(p[4]) ;
}

void QnFisheyeCalibrator::drawCircle(const QImage& frame, const QPoint& center, int radius)
{
    int w = frame.width();
    int h = frame.height();
    for (int y = 0; y < frame.height(); ++y)
    {
        for (int x = 0; x < frame.width(); ++x)
        {
            quint32* data = (quint32*) frame.bits() + y * frame.bytesPerLine()/4 + x;
            int idx = y/2 * frame.width()/2 + x/2;
            if (m_filteredFrame[idx])
                *data = -1;
            else
                *data = 0;
        }
    }

    /*
    float radius2 = (float) radius * (float) radius;
    quint32 result = 0;
    int yMin = qMax(0, center.y() - radius);
    int yMax = qMin(m_height-1, center.y() + radius);
    /*
    for (int y = yMin; y <= yMax; ++y)
    {
        float yOffs = center.y() - y;
        float xOffs = sqrt(radius2 - yOffs*yOffs);
        int xLeft = qMax(0, center.x() - int(xOffs + 0.5));
        int xRight = qMin(m_width-1, center.x() + int(xOffs + 0.5));

        quint32* data = (quint32*) frame.bits() + y * frame.bytesPerLine()/4 + xRight;
        *data = -1;

        data = (quint32*) frame.bits() + y * frame.bytesPerLine()/4 + xLeft;
        *data = -1;
        
        for (int x = xLeft; x <= xRight; ++x)
        {
            *data++ = -1;
        }
        
    }
    return result;
    */
}

int QnFisheyeCalibrator::findPixel(int y, int x, int xDelta)
{
    quint8* ptr = m_filteredFrame + y * m_width;
    for (int i = 0; i < m_width; ++i) {
        if (ptr[x])
            return x;
        x += xDelta;
    }
    return -1;
}

qreal QnFisheyeCalibrator::findElipse(qreal& newRadius)
{
    QPointF center(m_center.x() * m_width, m_center.y() * m_height);
    qreal height = m_radius * m_height;

    // scan lines
    QPointF p1, p2;
    for (int i = 0; i < 2; ++i) 
    {
        QVector<Distance> distances;
        qreal offset = (i==0) ? height*0.33 : height*0.66;
        int x1 = findPixel(center.y() + offset, m_width-1, -1);
        int x2 = findPixel(center.y() + offset, 0, 1);
        int x3 = findPixel(center.y() - offset, m_width-1, -1);
        int x4 = findPixel(center.y() - offset, 0, 1);
        distances << Distance(qAbs(center.x() - x1), QPointF(x1, center.y() + offset), RightBottom);
        distances << Distance(qAbs(center.x() - x2), QPointF(x2, center.y() + offset), LeftBottom);
        distances << Distance(qAbs(center.x() - x3), QPointF(x3, center.y() - offset), RightTop);
        distances << Distance(qAbs(center.x() - x4), QPointF(x4, center.y() - offset), LeftTop);
        qSort(distances);
        if (i == 0)
            p1 = QPointF(distances[3].pos);
        else
            p2 = QPointF(distances[3].pos);
    }

    p1 -= center;
    p2 -= center;

    qreal c = p2.x()*p2.x() * p1.y()*p1.y() - p1.x()*p1.x() * p2.y()*p2.y();
    qreal k = (p2.x()*p2.x() - p1.x()*p1.x());
    qreal b = sqrt(qAbs(c / k));
    qreal a = sqrt(qAbs(p1.x()*p1.x() * b*b / (b*b - p1.y()*p1.y())));
    newRadius = a;
    return a / b;
}

void QnFisheyeCalibrator::findCircleParams()
{
    QVector<Distance> distances;
    distances.resize(4);

    int rightEdge = m_width-1;
    for (int y = 0; y < m_height; ++y)
    {
        if (needToStop())
            return;

        int xLeft = findPixel(y, 0, 1);
        if (xLeft == -1)
            continue;
        qint64 distance = xLeft*xLeft + y*y;
        if (distance < distances[LeftTop].distance) {
            distances[LeftTop].distance = distance;
            distances[LeftTop].pos = QPointF(xLeft, y);
        }

        int xRight = findPixel(y, rightEdge, -1);
        if (xRight == -1)
            continue;
        distance = (rightEdge-xRight) * (rightEdge-xRight) + y*y;
        if (distance < distances[RightTop].distance) {
            distances[RightTop].distance = distance;
            distances[RightTop].pos = QPointF(xRight, y);
        }
    }

    int bottomEdge = m_height-1;
    for (int y = m_height-1; y > 0; --y)
    {
        if (needToStop())
            return;

        int xLeft = findPixel(y, 0, 1);
        if (xLeft == -1)
            continue;
        int distance = xLeft*xLeft + (bottomEdge-y) * (bottomEdge-y);
        if (distance < distances[LeftBottom].distance) {
            distances[LeftBottom].distance = distance;
            distances[LeftBottom].pos = QPoint(xLeft, y);
        }

        int rightEdge = m_width-1;
        int xRight = findPixel(y, rightEdge, -1);
        if (xRight == -1)
            continue;

        distance = (rightEdge-xRight) * (rightEdge-xRight) + (bottomEdge-y) * (bottomEdge-y);
        if (distance < distances[RightBottom].distance) {
            distances[RightBottom].distance = distance;
            distances[RightBottom].pos = QPoint(xRight, y);
        }
    }

    qSort(distances);
    if (distances[3].distance == INT64_MAX || distances[0].distance == 0) {
        emit finished (ErrorNotFisheyeImage);
        return; // not found
    }

    // remove most far point
    int leftDistance = distances[1].distance - distances[0].distance;
    int rightDistance = distances[3].distance - distances[2].distance;
    int startIdx = 0;
    if (leftDistance > rightDistance)
        startIdx = 1;

    QPointF a1(distances[startIdx].pos);
    QPointF a2(distances[startIdx+1].pos);
    QPointF a3(distances[startIdx+2].pos);

    if (a1.x() == a2.x())
        qSwap(a2, a3);
    else if (a2.x() == a3.x())
        qSwap(a1, a3);
    else if (a1.y() == a2.y())
        qSwap(a1, a3);

    qreal ma = (a2.y() - a1.y()) / (a2.x() - a1.x());
    qreal mb = (a3.y() - a2.y()) / (a3.x() - a2.x());

    if ((ma == 0 && mb == 0) || ma > 1e9 || mb > 1e9 || ma < -1e9 || mb < -1e9) {
        emit finished (ErrorNotFisheyeImage);
        return;
    }

    qreal centerX = ma * mb * (a1.y() - a3.y()) + mb * (a1.x() + a2.x()) - ma * (a2.x() + a3.x());
    centerX /= 2 * (mb - ma);
    qreal centerY = -(1.0 / ma) * (centerX - (a1.x() + a2.x())/2.0) + (a1.y() + a2.y())/2.0;

    qreal dx = centerX - a1.x();
    qreal dy = centerY - a1.y();
    int radius = sqrt(dx*dx + dy*dy) + 0.5;

    setCenter(QPointF(centerX / (qreal) m_width, centerY / (qreal)m_height));
    setRadius(radius / (qreal) m_width);

    // check for horizontal stretch
    qreal newRadius = 0.0;
    qreal stretch = findElipse(newRadius);
    if (qBetween(1.1, stretch, 2.0)) {
        setRadius(newRadius / (qreal) m_width);
        setHorizontalStretch(stretch);
    }
    else {
        setHorizontalStretch(1.0);
    }

    emit finished(NoError);
}

/*
int QnFisheyeCalibrator::findYThreshold(QImage frame)
{
    // Use adaptive binarisation to find optimal YThreshold value
    
    // 1. build hystogram
    
    int hystogram[256];
    memset(hystogram, 0, sizeof(hystogram));
    for (int y = 0; y < m_width; ++y)
    {
        quint32* curPtr = (quint32*) (frame.bits() + y * frame.bytesPerLine());
        for (int x = 0; x < frame.width()/4; ++x)
        {
            quint32 value = *curPtr++;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
        }
    }

    // 2. split data to two buckets

    // 2.1 calculate initial range
    int left = 0;
    int right = 255;
    while (hystogram[left] == 0 && left < 256)
        left++;
    while (hystogram[right] == 0 && right > 0)
        right--;

    if (left > right)
        return 0; // not data in the source frame
    
    while (left < right)
    {
        // 2.2 calculate center for left and right bucket
        int mid = (left+right+1)/2;

        quint32 leftSum = 0;
        for (int i = left; i < mid; ++i)
            leftSum += hystogram[i];
        quint32 halfSum = 0;

        for (; left < mid && halfSum < leftSum/2; ++left)
            halfSum += hystogram[left];

        quint32 rightSum = 0;
        for (int i = mid; i <= right; ++i)
            rightSum += hystogram[i];
        halfSum = 0;
        for (; right >= mid && halfSum < rightSum/2; --right)
            halfSum += hystogram[right];
    }
    return left;
}
*/

int QnFisheyeCalibrator::findYThreshold(QImage frame)
{
    int w = frame.width();
    int h = frame.height();
    static const int MAX_Y_THRESHOLD = 64;
    static const int MIN_Y_THRESHOLD = 32;
    static const int DETECT_BORDER_DELTA = 32;
    QVector<int> borders;
    for (int y = 0; y < frame.height(); ++y)
    {
        const quint8* line = frame.bits() + y * frame.bytesPerLine();
        for (int x = 0; x < frame.width() - 2; ++x)
        {
            if (line[x] >= MAX_Y_THRESHOLD)
                break;
            else if (line[x+1] - line[x] >= DETECT_BORDER_DELTA) {
                //borders <<(line[x+1] - line[x])/2 + line[x];
                borders << line[x+1]-1;
                break;
            }
            else if (line[x+2] - line[x] >= DETECT_BORDER_DELTA) {
                //borders <<(line[x+2] - line[x])/2 + line[x];
                borders << line[x+2]-1;
                break;
            }
        }
    }
    if (borders.isEmpty())
        return MAX_Y_THRESHOLD; // default value
    qSort(borders);
    int result = borders[borders.size()/2];

    return qBound(MIN_Y_THRESHOLD, result, MAX_Y_THRESHOLD);

#if 0
    // Use adaptive binarisation to find optimal YThreshold value

    // 1. build hystogram

    int hystogram[256];
    memset(hystogram, 0, sizeof(hystogram));
    for (int y = 0; y < m_height; ++y)
    {
        quint32* curPtr = (quint32*) (frame.bits() + y * frame.bytesPerLine());
        for (int x = 0; x < frame.width()/4; ++x)
        {
            quint32 value = *curPtr++;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
        }
    }


    // 2.1 calculate initial range
    int left = 0;
    int right = 255;
    while (hystogram[left] == 0 && left < 256)
        left++;
    while (hystogram[right] == 0 && right > 0)
        right--;

    int midPoint = 0;
    qint64 sum = 0;
    qint64 halfSquare = m_width * m_height / 2;
    int midPos = left;
    for (; midPos <= right; ++midPos)
    {
        sum += hystogram[midPos];
        if (sum >= halfSquare)
            break;
    }
    int result = midPos / 2 + left;
    if (result < 16)
        return -1;
    else
        return qBound(28, result, 64);
#endif
}

void QnFisheyeCalibrator::analyseFrameAsync(QImage frame)
{
    if (isRunning())
        stop();
    m_analysedFrame = frame;
    start();
}

void QnFisheyeCalibrator::run()
{
    analyseFrame(m_analysedFrame);
}

void QnFisheyeCalibrator::analyseFrame(QImage frame)
{
    frame = frame.scaled(frame.width() / 2, frame.height() / 2); // addition filtering

    if (frame.format() != QImage::Format_Indexed8) 
    {
        // copy data to the tmp buffer because source buffer may be unaligned
        int inputNumBytes = frame.bytesPerLine()*frame.height();
        uchar* inputData[4];
        inputData[0] = static_cast<uchar*>(qMallocAligned(inputNumBytes, 32));
        inputData[1] = inputData[2] = inputData[3] = 0;
        memcpy(inputData[0], frame.bits(), inputNumBytes);

        int roundWidth = qPower2Ceil((unsigned)frame.width(),32);
        int numBytes = avpicture_get_size(PIX_FMT_GRAY8, roundWidth, frame.height());
        qFreeAligned(m_grayImageBuffer);
        m_grayImageBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));

        SwsContext* scaleContext = sws_getContext(frame.width(), frame.height(), PIX_FMT_RGBA, 
            frame.width(), frame.height(), PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);

        AVPicture dstPict;
        avpicture_fill(&dstPict, m_grayImageBuffer, PIX_FMT_GRAY8, roundWidth, frame.height());

        int inputLinesize[4];
        inputLinesize[0] = frame.bytesPerLine();
        inputLinesize[1] = inputLinesize[2] = inputLinesize[3] = 0;

        QImage newFrame(m_grayImageBuffer, frame.width(), frame.height(), roundWidth, QImage::Format_Indexed8);
        sws_scale(scaleContext, inputData, inputLinesize, 0, frame.height(), dstPict.data, dstPict.linesize);
        sws_freeContext(scaleContext);
        qFreeAligned(inputData[0]);

        frame = newFrame;
    }

    m_width = frame.width();
    m_height = frame.height();

    const int Y_THRESHOLD = findYThreshold(frame);

    if (Y_THRESHOLD == -1) {
        emit finished (ErrorTooLowLight);
        return;
    }


    const quint8* curPtr = (const quint8*) frame.bits();
    delete m_filteredFrame;
    m_filteredFrame = new quint8[frame.width() * frame.height()];
    
    quint8* dstPtr = m_filteredFrame;
    int srcYStep = frame.bytesPerLine();

    std::vector<quint8> data;
    data.resize(9);

    for (int x = 0; x < m_width ; ++x) {
        *dstPtr++ = 0;
        curPtr++;
    }
    curPtr += srcYStep - m_width;

    for (int y = 1; y < m_height - 1; ++y)
    {
        if (needToStop())
            return;

        *dstPtr++ = 0;
        curPtr++;
        for (int x = 1; x < m_width - 1; ++x)
        {
            data[0] = curPtr[srcYStep-1];
            data[1] = curPtr[-srcYStep];
            data[2] = curPtr[-srcYStep+1];
            data[3] = curPtr[-1];
            data[4] = curPtr[0];
            data[5] = curPtr[1];
            data[6] = curPtr[-srcYStep-1];
            data[7] = curPtr[srcYStep];
            data[8] = curPtr[srcYStep+1];

            *dstPtr++ = opt_med9(&data[0]) > Y_THRESHOLD ? 240 : 0;
            curPtr++;
        }
        *dstPtr++ = 0;
        curPtr += srcYStep - m_width + 1;
    }

    for (int x = 0; x < m_width ; ++x) {
        *dstPtr++ = 0;
    }


    findCircleParams();

#if 0
    // update source frame for test purpose
    QPoint center = QPoint(m_center.x() * m_width, m_center.y() * m_height);
    int redius = m_radius * m_width;
    drawCircle(m_analysedFrame, center, redius);
    //memset(frame->data[1], 0x80, frame->height*frame->linesize[1]/2);
    //memset(frame->data[2], 0x80, frame->height*frame->linesize[2]/2);
    /*
    for (int y = 0; y < frame->height; ++y)
    {
        memcpy(frame->data[0] + frame->linesize[0]*y, m_filteredFrame + frame->width*y, frame->width);
    }
    */
#endif

}
