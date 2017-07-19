#include "car_detection_plugin.h"
#if defined(ENABLE_NVIDIA_ANALYTICS)

#include <limits.h>
#include <cmath>
#include <fstream>
#include <ios>

#include <cuda_runtime_api.h>

#include <QtCore/QCoreApplication>

#include <utils/common/synctime.h>

#define OUTPUT_PREFIX "[analytics::CarDetectionPlugin] "
#include <nx/utils/debug_utils.h>

#include <analytics/common/config.h>


namespace nx {
namespace analytics {

namespace {

static const QString kCarDetectionPluginId = lit("car_detection_plugin");
static const std::chrono::milliseconds kTimeoutMs(30);
static const int kGroupThreshold = 3;
static const double kEps = 0.2;

static const int kBboxBufferIndex = 1;
static const int kCovBufferIndex = 2;

static const float helnetScale[4] = {-640, -480, 640, 480};

static const double kThreshold = 0.8;

static const QString kDeployFilePath = lit(
    "/nvidia_models/GoogleNet-modified.prototxt");
static const QString kModelFilePath = lit(
    "/nvidia_models/GoogleNet-modified-online_iter_30000.caffemodel");

static const QString kInputBlobName = lit("data");
static const QString kOutputCovBlobName = lit("coverage");
static const QString kOutputBboxBlobName = lit("bboxes");
static const std::vector<QString> kOutputBlobs = {kOutputBboxBlobName, kOutputCovBlobName};

static const int kStride = 4;
static const double kSimilarityThreshold = 0.05;
static const int kDetectionPeriod = 3;

} // namespace

CarDetectionPlugin::CarDetectionPlugin(const QString& id):
    m_lastFrameTime(AV_NOPTS_VALUE),
    m_gieExecutor(nullptr),
    m_scaleContext(nullptr),
    m_scaleBuffer(nullptr),
    m_scaleBufferSize(0),
    m_gotData(false),
    m_prevResolution(0, 0),
    m_hasValidEngine(false),
    m_frameCounter(kDetectionPeriod),
    m_tegraVideo(nullptr)
{
    if (conf.enableTegraVideo)
    {
        OUTPUT << "Using TegraVideo: conf.enableTegraVideo=true";
        TegraVideo::Params params;
        std::string idStr = id.toStdString();
        params.id = idStr.c_str();
        params.modelFile = conf.modelFile;
        params.deployFile = conf.deployFile;
        params.cacheFile = conf.cacheFile;
        m_tegraVideo.reset(TegraVideo::create(params));
        NX_CRITICAL(m_tegraVideo, "Unable to initialize tegra_video.");
    }
    else
    {
        OUTPUT << "Not using TegraVideo: conf.enableTegraVideo=false";
        m_gieExecutor.reset(new GieExecutor(kInputBlobName, kOutputBlobs));
        m_hasValidEngine = m_gieExecutor->buildEngine();
    }

    m_timer.start();
}

CarDetectionPlugin::~CarDetectionPlugin()
{
    if (m_scaleBuffer)
        qFreeAligned(m_scaleBuffer);
}

QString CarDetectionPlugin::id() const
{
    return kCarDetectionPluginId;
}

bool CarDetectionPlugin::hasMetadata()
{
    if (!conf.enableTegraVideo)
        return m_gotData;

    int64_t ptsUs = 0;
    
    return m_tegraVideo->hasMetadata();
}

bool CarDetectionPlugin::pushFrame(const CLVideoDecoderOutputPtr& data)
{
    if (!m_hasValidEngine)
        return true;

    if (true /* m_frameCounter >= kDetectionPeriod */)
    {
        m_gotData = convertAndPushImageToGie(data.data());
        m_frameCounter = -1;
    }

    ++m_frameCounter;
    return true;
}

namespace {

QByteArray loadFileToByteArray(const QString& filename)
{
    QByteArray result;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        PRINT << "Unable to open file for reading: [" << filename << "]";
        return result;
    }

    result = file.readAll();
    file.close();

    if (result.isEmpty())
        PRINT << "Unable to read from file: " << filename;

    OUTPUT << "Substituted frame from file: " << filename;
    return result;
}

} // namespace

bool CarDetectionPlugin::pushFrame(const QnCompressedVideoDataPtr& compressedFrameData)
{
    NX_ASSERT(conf.enableTegraVideo, lit("ini.enableTegraVideo is off"));

    TegraVideo::CompressedFrame compressedFrame;
    QByteArray dataFromFile;
    if (conf.substituteFramesFilePrefix[0] != '\0')
    {
        // Instead of the received data, read a raw frame from the file.

        static int frameNumber = 0;
        ++frameNumber;

        dataFromFile = QByteArray(
            loadFileToByteArray(
                lit("%1.%2").arg(QLatin1String(conf.substituteFramesFilePrefix))
                    .arg(frameNumber)));

        compressedFrame.data = (const uint8_t*) dataFromFile.constData();
        compressedFrame.dataSize = dataFromFile.size();
    }
    else
    {
        compressedFrame.data = (const uint8_t*) compressedFrameData->data();
        compressedFrame.dataSize = (int) compressedFrameData->dataSize();   
    }

    compressedFrame.ptsUs = compressedFrameData->timestamp;

    if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
    {
        PRINT << "ERROR: pushCompressedFrame(dataSize: " << compressedFrame.dataSize
            << ", ptsUs: " << compressedFrame.ptsUs << ") -> false";
        return false;
    }

    return true;
}

QnAbstractCompressedMetadataPtr CarDetectionPlugin::getNextMetadata()
{
    QnObjectDetectionMetadataPtr result;
    int64_t pts;
    int count = 0;

    if (conf.enableTegraVideo)
    {
        m_tegraVideo->pullRectsForFrame(m_tegraVideoRects, kMaxRectanglesNumber, &count, &pts);
        result = tegraVideoRectsToObjectDetectionMetadata(m_tegraVideoRects, count);
    }
    else
    {
        if (!m_hasValidEngine)
            return nullptr;

        m_gotData = false;
        result = parseGieOutput();
    }

    if (result)
    {
        auto compressed = result->serialize();
        compressed->timestamp = pts > 0 ? pts : qnSyncTime->currentMSecsSinceEpoch();
        compressed->channelNumber = m_channel;
        compressed->m_duration = 1000 * 1000 * 1000; //< 1000 seconds.
        return compressed;
    }

    return nullptr;
}

void CarDetectionPlugin::reset()
{
    m_tegraVideo.reset();
}

bool CarDetectionPlugin::convertAndPushImageToGie(const AVFrame* srcFrame)
{
    static QElapsedTimer timer2;
    timer2.restart();
    static const AVPixelFormat dstAvFormat = AV_PIX_FMT_RGBA;
    const int dstWidth = 960;
    const int dstHeight = 540;

    if (!srcFrame->width || !srcFrame->height)
        return false;

    bool resolutionChanged = srcFrame->width != m_prevResolution.width()
        || srcFrame->height != m_prevResolution.height();

    if (!m_scaleContext || resolutionChanged)
    {
        m_prevResolution.setWidth(srcFrame->width);
        m_prevResolution.setHeight(srcFrame->height);

        m_scaleContext = sws_getContext(
            srcFrame->width, srcFrame->height, (AVPixelFormat)srcFrame->format,
            dstWidth, dstHeight, dstAvFormat,
            SWS_BICUBIC, NULL, NULL, NULL);

        if (!m_scaleContext)
            return false;
    }

    int numBytes = avpicture_get_size(dstAvFormat, dstWidth, dstHeight);
    if (numBytes <= 0)
        return false; //< can't allocate frame

    numBytes += FF_INPUT_BUFFER_PADDING_SIZE; //< extra alloc space due to ffmpeg doc

    if (numBytes != m_scaleBufferSize)
    {
        if (m_scaleBuffer)
            qFreeAligned(m_scaleBuffer);

        m_scaleBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));
        m_scaleBufferSize = numBytes;
    }

    AVPicture dstPict;
    avpicture_fill(
        &dstPict,
        m_scaleBuffer,
        dstAvFormat,
        dstWidth,
        dstHeight);

    sws_scale(
        m_scaleContext,
        srcFrame->data, srcFrame->linesize,
        0, srcFrame->height,
        dstPict.data, dstPict.linesize);

    OUTPUT << "Scaling took " << timer2.elapsed();

    auto inputBuffer = m_gieExecutor->getInputBuffer();
    //auto channelOffset = m_gieExecutor->getNetWidth() * m_gieExecutor->getNetHeight();
    auto channelNumber = m_gieExecutor->getChannelNumber();
    auto netWidth = m_gieExecutor->getNetWidth();
    auto netHeight = m_gieExecutor->getNetHeight();

    static QElapsedTimer timer;
    timer.restart();

    // copy buffer into input_buf
    for (int i = 0; i < channelNumber; ++i)
    {
        for (int j = 0; j < netHeight; ++j)
        {
            for (int k = 0; k < netWidth; ++k)
            {
                //int totalOffset = batch_offset + channelOffset * i + j * netWidth + k;

                int totalOffset = (i * netHeight + j) * netWidth + k; //< from Caffe blob docs

                inputBuffer[totalOffset] =
                    (float) (*(dstPict.data[0] + j * dstPict.linesize[0] + k * 4 + 3 - i - 1));
            }
        }
    }

    static QElapsedTimer timer3;
    timer3.restart();

    m_gieExecutor->doInference();

    return true;
}

QnObjectDetectionMetadataPtr CarDetectionPlugin::parseGieOutput() const
{
    return parseBbox();
}

QnObjectDetectionMetadataPtr CarDetectionPlugin::parseBbox() const
{
    static QElapsedTimer timer;
    timer.restart();
    std::vector<cv::Rect> rectangles;

    auto outputDims = m_gieExecutor->getOutputDimensions(kCovBufferIndex);
    auto outputDimsBbox = m_gieExecutor->getOutputDimensions(kBboxBufferIndex);

    auto covBuffer = (float*)m_gieExecutor->getBuffer(kCovBufferIndex);
    auto bboxBuffer = (float*)m_gieExecutor->getBuffer(kBboxBufferIndex);

    if (!bboxBuffer || !covBuffer)
        return nullptr;

    int gridsize = outputDims.h * outputDims.w;

    auto netWidth = m_gieExecutor->getNetWidth();
    auto netHeight = m_gieExecutor->getNetHeight();

    float* outputX1 = bboxBuffer
        /* + outputDimsBbox.c * outputDimsBbox.h * outputDimsBbox.w * batch_th */;
    float* outputY1 = outputX1 + outputDimsBbox.h * outputDimsBbox.w;
    float* outputX2 = outputY1 + outputDimsBbox.h * outputDimsBbox.w;
    float* outputY2 = outputX2 + outputDimsBbox.h * outputDimsBbox.w;

    int gridOffset = 0 /* outputDims.c * outputDims.h * outputDims.w * batch_th */;

    for (int i = 0; i < gridsize; ++i)
    {
        if (covBuffer[gridOffset + i] >= kThreshold)
        {
            int gX = i % outputDims.w;
            int gY = i / outputDims.w;
            int iX = gX * kStride ;
            int iY = gY * kStride ;
            int rectx1 = outputX1[i]+iX;
            int recty1 = outputY1[i]+iY;
            int rectx2 = outputX2[i]+iX;
            int recty2 = outputY2[i]+iY;
            if (rectx1 < 0)
                rectx1 = 0;
            if (rectx2 < 0)
                rectx2 = 0;
            if (recty1 < 0)
                recty1 = 0;
            if (recty2 < 0)
                recty2 = 0;

            if (rectx1 >= netWidth)
                rectx1 = netWidth - 1;
            if (rectx2 >= netWidth)
                rectx2 = netWidth - 1;
            if (recty1 >= netHeight)
                recty1 = netHeight - 1;
            if (recty2 >= netHeight)
                recty2 = netHeight - 1;

            rectangles.push_back(cv::Rect(rectx1, recty1, rectx2 - rectx1, recty2 - recty1));
        }
    }

    cv::groupRectangles(rectangles, kGroupThreshold, kEps);

    return toObjectDetectionMetadata(rectangles);
}

QnObjectDetectionMetadataPtr CarDetectionPlugin::parseHelBbox() const
{
    std::vector<cv::Rect> rectangles;

    auto bboxBuffer = (float*) m_gieExecutor->getBuffer(kBboxBufferIndex);
    auto covBuffer = (float*) m_gieExecutor->getBuffer(kCovBufferIndex);

    auto outputDimsBbox = m_gieExecutor->getOutputDimensions(kBboxBufferIndex);
    auto outputDims = m_gieExecutor->getOutputDimensions(kCovBufferIndex);

    auto netWidth = m_gieExecutor->getNetWidth();
    auto netHeight = m_gieExecutor->getNetHeight();

    int gridX = outputDims.w;
    int gridY = outputDims.h;
    int gridSize = gridX * gridY;

    float* outputX1 = bboxBuffer
            /*+ c * 4 * outputDimsBbox.h * outputDimsBbox.w /*+
            outputDimsBbox.c * outputDimsBbox.h *
            outputDimsBbox.w * batch_th*/;
    float* outputY1 = outputX1 + outputDims.h * outputDims.w;
    float* outputX2 = outputY1 + outputDims.h * outputDims.w;
    float* outputY2 = outputX2 + outputDims.h * outputDims.w;

    int gridOffset = 0/*outputDims.c * outputDims.h * outputDims.w * batch_th*/;

    for (int i = 0; i < gridSize; i++)
    {
        if (covBuffer[gridOffset /*+ c * gridSize*/ + i] >= kThreshold)
        {
            int gX = i % gridX;
            int gY = i / gridX;
            int iX = gX * kStride;
            int iY = gY * kStride;
            int rectx1 = helnetScale[0] * outputX1[i] + iX;
            int recty1 = helnetScale[1] * outputY1[i] + iY;
            int rectx2 = helnetScale[2] * outputX2[i] + iX;
            int recty2 = helnetScale[3] * outputY2[i] + iY;

            if (rectx1 < 0)
                rectx1 = 0;
            if (rectx2 < 0)
                rectx2 = 0;
            if (recty1 < 0)
                recty1 = 0;
            if (recty2 < 0)
                recty2 = 0;

            if (rectx1 >= netWidth)
                rectx1 = netWidth - 1;
            if (rectx2 >= netWidth)
                rectx2 = netWidth - 1;
            if (recty1 >= netHeight)
                recty1 = netHeight - 1;
            if (recty2 >= netHeight)
                recty2 = netHeight - 1;

            rectangles.push_back(
                cv::Rect(
                    rectx1, recty1,
                    rectx2 - rectx1, recty2 - recty1));
        }
    }

    cv::groupRectangles(rectangles, kGroupThreshold, kEps);
    return toObjectDetectionMetadata(rectangles);
}

QnObjectDetectionMetadataPtr CarDetectionPlugin::toObjectDetectionMetadata(
    const std::vector<cv::Rect>& rectangles) const
{
    static QElapsedTimer timer;
    timer.restart();
    QnObjectDetectionMetadataPtr metadata =
        std::make_shared<QnObjectDetectionMetadata>();

    auto netHeight = m_gieExecutor->getNetHeight();
    auto netWidth = m_gieExecutor->getNetWidth();

    for (const auto& rect: rectangles)
    {
        if (rect.width < 10 || rect.height < 10)
            continue;

        QnObjectDetectionInfo info;
        QnPoint2D topLeft((double) rect.x / netWidth, (double) rect.y / netHeight);
        QnPoint2D bottomRight(
            ((double) rect.x + rect.width) / netWidth,
            ((double) rect.y + rect.height) / netHeight);
        info.boundingBox = QnRect(topLeft, bottomRight);


        if (auto sameObject = findAndMarkSameObjectInCache(info.boundingBox))
        {
            info.objectId = sameObject->id;
            updateObjectInCache(info.objectId, info.boundingBox);
            metadata->detectedObjects.push_back(info);
        }
        else
        {
            info.objectId = QnUuid::createUuid();
            addObjectToCache(info.objectId, info.boundingBox);
            metadata->detectedObjects.push_back(info);
        }
    }

    removeExpiredObjectsFromCache();
    addNonExpiredObjectsFromCache(metadata->detectedObjects);
    unmarkFoundObjectsInCache();

    OUTPUT << "Conversion of cvRects to metadata took " << timer.elapsed();
    return metadata;
}

QnObjectDetectionMetadataPtr CarDetectionPlugin::tegraVideoRectsToObjectDetectionMetadata(
    const TegraVideo::Rect* rectangles, int rectangleCount) const
{
    static QElapsedTimer timer;
    timer.restart();
    QnObjectDetectionMetadataPtr metadata =
        std::make_shared<QnObjectDetectionMetadata>();

    bool isPedNet = strcmp(conf.netType, "pednet") == 0;
    qDebug() << "######### GOT RECTANGLES:" << rectangleCount;
    for (auto i = 0; i < rectangleCount; ++i)
    {
        const auto& rect = rectangles[i];
        auto netWidth = 0;
        auto netHeight = 0;

        if (isPedNet)
        {
            netWidth = nx::analytics::kPedNetWidth;
            netHeight = nx::analytics::kPedNetHeight;
        }
        else
        {
            netWidth = nx::analytics::kCarNetWidth;
            netHeight = nx::analytics::kCarNetHeight;
        }

        if (rect.width < (float) conf.minRectWidth / netWidth
            || rect.height < (float) conf.minRectHeight / netHeight)
        {
            continue;
        }

        QnObjectDetectionInfo info;
        QnPoint2D topLeft(rect.x, rect.y);
        QnPoint2D bottomRight(rect.x + rect.width, rect.y + rect.height);
        info.boundingBox = correctRectangle(QnRect(topLeft, bottomRight));

        if (isPedNet)
        {
            info.objectId = QnUuid::createUuid();
            metadata->detectedObjects.push_back(info);
        }
        else
        {
            if (auto sameObject = findAndMarkSameObjectInCache(info.boundingBox))
            {
                info.objectId = sameObject->id;
                updateObjectInCache(info.objectId, info.boundingBox);
                metadata->detectedObjects.push_back(info);
            }
            else
            {
                info.objectId = QnUuid::createUuid();
                addObjectToCache(info.objectId, info.boundingBox);
                metadata->detectedObjects.push_back(info);
            }
        }
    }

    if (!isPedNet)
    {

        removeExpiredObjectsFromCache();
        addNonExpiredObjectsFromCache(metadata->detectedObjects);
        unmarkFoundObjectsInCache();

        decltype(metadata->detectedObjects) filtered;

        for (const auto& obj: metadata->detectedObjects)
        {
            const auto& b = obj.boundingBox;

            auto width = b.bottomRight.x - b.topLeft.x;
            auto height = b.bottomRight.y - b.topLeft.y;

            if ((height > ((double)conf.maxObjectHeight / 100))
                || (width > (double)conf.maxObjectWidth / 100)
                || (width < 0.02)
                || (height < 0.02))
            {
                continue;
            }

            auto rectCenter = rectangleCenter(b);
            if (rectCenter.y() < 0.9 && rectCenter.x() > 0.85 ||
                rectCenter.y() < 0.5 && rectCenter.x() > 0.80)
            {
                continue;
            }

            filtered.push_back(obj);

        }

        std::swap(metadata->detectedObjects, filtered);
    }

    PRINT << "-----------------";
    for (const auto& obj: metadata->detectedObjects)
    {
        PRINT << "OBJECT ID: "<< obj.objectId << " ("
            << obj.boundingBox.topLeft.x << " "
            << obj.boundingBox.topLeft.y << ") ("
            << obj.boundingBox.bottomRight.x << " "
            << obj.boundingBox.bottomRight.y << ")";

    }
    PRINT << "-----------------";


    return metadata;
}

void CarDetectionPlugin::unmarkFoundObjectsInCache() const
{
    for (auto& item: m_cachedObjects)
        item.second.found = false;
}

boost::optional<CarDetectionPlugin::CachedObject> CarDetectionPlugin::findAndMarkSameObjectInCache(
    const QnRect& boundingBox) const
{
    double minDistance = std::numeric_limits<double>::max();
    auto closestRect = m_cachedObjects.end();

    for (auto itr = m_cachedObjects.begin(); itr != m_cachedObjects.end(); ++itr)
    {
        if (itr->second.found)
            continue;

        auto& rect = itr->second.rect;
        auto speed = itr->second.speed;
        double currentDistance = std::numeric_limits<double>::max();
        if (conf.applySpeedForDistanceCalculation)
        {
            QVector2D speed = itr->second.speed;
            if (speed.isNull())
                speed = QVector2D(predictXSpeedForRectangle(itr->second.rect), (double) conf.defaultYSpeed / 1000);

            currentDistance = distance(
                applySpeedToRectangle(rect, speed), boundingBox);
        }
        else
        {
            currentDistance = distance(rect, boundingBox);
        }

        if (currentDistance < minDistance)
        {
            closestRect = itr;
            minDistance = currentDistance;
        }
    }

    auto minimalSimilarityDistance = (double) conf.similarityThreshold / 1000;
    bool sameObjectWasFound = closestRect != m_cachedObjects.end() 
        && minDistance < minimalSimilarityDistance;

    if (sameObjectWasFound)
    {
        closestRect->second.found = true;
        return closestRect->second;
    }

    return boost::none;
}

void CarDetectionPlugin::addObjectToCache(const QnUuid& id, const QnRect& boundingBox) const
{
    CachedObject object;
    object.id = id;
    object.rect = boundingBox;
    object.lifetime = conf.defaultObjectLifetime;
    object.found = true;

    m_cachedObjects[id] = object;
}

void CarDetectionPlugin::updateObjectInCache(const QnUuid& id, const QnRect& boundingBox) const
{
    auto itr = m_cachedObjects.find(id);
    if (itr != m_cachedObjects.end())
    {
        auto& cached = itr->second;

        cached.speed = calculateSpeed(cached.rect, boundingBox);
        cached.rect = boundingBox;

        auto borderThreshold = (double)conf.bottomBorderBound / 100;
        if (cached.rect.bottomRight.y > borderThreshold || 
            cached.rect.bottomRight.x > borderThreshold ||
            cached.rect.topLeft.x < 1 - borderThreshold)
        {
            cached.lifetime = conf.bottomBorderLifetime;
        }
        else
        {
            cached.lifetime = conf.defaultObjectLifetime;
        }

    }
}

void CarDetectionPlugin::removeExpiredObjectsFromCache() const
{
    for (auto itr = m_cachedObjects.begin(); itr != m_cachedObjects.end();)
    {
        auto& lifetime = itr->second.lifetime;
        if (lifetime == 0)
        {
            itr = m_cachedObjects.erase(itr);
        }
        else
        {
            --lifetime;
            ++itr;
        }
    }
}

void CarDetectionPlugin::addNonExpiredObjectsFromCache(std::vector<QnObjectDetectionInfo>& objects) const
{
    for (auto& item: m_cachedObjects)
    {
        if (!item.second.found)
        {
            QnObjectDetectionInfo info;
            auto& cached = item.second;

            if (conf.applySpeedToCachedRectangles)
            {   
                if (cached.speed.x() < cached.speed.y() || cached.speed.isNull())
                {
                    cached.rect = applySpeedToRectangle(cached.rect, QVector2D(
                        predictXSpeedForRectangle(cached.rect),
                        (double)conf.defaultYSpeed / 1000));

                    OUTPUT << "(addNonExpiredObjects) " << cached.id << " ("
                        << cached.rect.topLeft.x << " "
                        << cached.rect.topLeft.y << ") ("
                        << cached.rect.bottomRight.x << " "
                        << cached.rect.bottomRight.y << ")";
                }
            }
            
            info.boundingBox = cached.rect;
            info.objectId = cached.id;
            objects.push_back(info);
        }
    }
}

QnRect CarDetectionPlugin::applySpeedToRectangle(const QnRect& rectangle, const QVector2D& speed) const
{
    QnRect result = rectangle;
    auto xOffset = speed.x();
    auto yOffset = speed.y();

    result.topLeft.x = std::max(std::min(result.topLeft.x + xOffset, 1.0), 0.0);
    result.bottomRight.x = std::max(std::min(result.bottomRight.x + xOffset, 1.0), 0.0);
    result.topLeft.y = std::max(std::min(result.topLeft.y + yOffset, 1.0), 0.0);
    result.bottomRight.y = std::max(std::min(result.bottomRight.y + yOffset, 1.0), 0.0);

    return result;
}

QVector2D CarDetectionPlugin::calculateSpeed(
    const QnRect& previousPosition,
    const QnRect& currentPosition) const
{
    auto firstCenterX = (previousPosition.topLeft.x + previousPosition.bottomRight.x) / 2;
    auto firstCenterY = (previousPosition.topLeft.y + previousPosition.bottomRight.y) / 2;
    auto secondCenterX = (currentPosition.topLeft.x + currentPosition.bottomRight.x) / 2;
    auto secondCenterY = (currentPosition.topLeft.y + currentPosition.bottomRight.y) / 2;

    QVector2D speed;

    speed.setX(secondCenterX - firstCenterX);
    speed.setY(secondCenterY - firstCenterY);

    return speed;
}

double CarDetectionPlugin::distance(const QnRect& first, const QnRect& second) const
{
    auto firstCenterX = (first.topLeft.x + first.bottomRight.x) / 2;
    auto firstCenterY = (first.topLeft.y + first.bottomRight.y) / 2;
    auto secondCenterX = (second.topLeft.x + second.bottomRight.x) / 2;
    auto secondCenterY = (second.topLeft.y + second.bottomRight.y) / 2;

    auto rectCenterDistance = std::sqrt(
        std::pow(firstCenterX - secondCenterX, 2) +
        std::pow(firstCenterY - secondCenterY, 2));

    auto centerVector = QVector2D(
        secondCenterX - firstCenterX,
        secondCenterY - secondCenterY);

    // Hack. We consider only objects that move forward to the viewer or have small 
    // speed in opposite direction.
    auto dotProduct = QVector2D::dotProduct(
        QVector2D(0.0, 1.0),
        centerVector);

    if (dotProduct < 0 && rectCenterDistance > conf.maxBackVectorLength)
        return std::numeric_limits<double>::max();

    return rectCenterDistance - dotProduct;
}

QPointF CarDetectionPlugin::rectangleCenter(const QnRect& rect) const
{
    QPointF center;
    center.setX((rect.topLeft.x + rect.bottomRight.x) / 2);
    center.setY((rect.topLeft.y + rect.bottomRight.y) / 2);

    return center;
}

QnRect CarDetectionPlugin::correctRectangle(const QnRect& rect) const
{
    double xCorrection = 0; 
    double yCorrection = 0;

    auto center = rectangleCenter(rect);
    auto x = center.x();
    auto y = center.y();

    double xFirstZoneCorrection = (double) conf.xFirstZoneCorrection / 1000;
    double yFirstZoneCorrection = (double) conf.yFirstZoneCorrection / 1000;

    double xSecondZoneCorrection = (double) conf.xSecondZoneCorrection / 1000;
    double ySecondZoneCorrection = (double) conf.ySecondZoneCorrection / 1000;

    double xThirdZoneCorrection = (double) conf.xThirdZoneCorrection / 1000;
    double yThirdZoneCorrection = (double) conf.yThirdZoneCorrection / 1000;

    double firstZoneBound = (double) conf.firstZoneBound / 100;
    double secondZoneBound = (double) conf.secondZoneBound / 100;

    if (x < firstZoneBound)
    {
        xCorrection = xFirstZoneCorrection;
        yCorrection = yFirstZoneCorrection;
    }
    else if (x < secondZoneBound)
    {
        xCorrection = xSecondZoneCorrection;
        yCorrection = ySecondZoneCorrection;
    }
    else
    {
        xCorrection = xThirdZoneCorrection;
        yCorrection = yThirdZoneCorrection;
    }

    QVector2D speed(xCorrection, yCorrection);

    return applySpeedToRectangle(rect, speed);
}

double CarDetectionPlugin::predictXSpeedForRectangle(const QnRect& rect) const
{
    auto x = rectangleCenter(rect).x();

    if (x < (double) conf.firstZoneBound / 100)
        return (double) conf.xFirstZoneCorrection / 1000;
    else if (x > (double) conf.secondZoneBound / 100)
        return (double) conf.xThirdZoneCorrection / 1000;
    else
        return (double) conf.xSecondZoneCorrection / 1000;
}

} // namespace analytics
} // namespace nx

#endif // defined(ENABLE_NVIDIA_ANALYTICS)
