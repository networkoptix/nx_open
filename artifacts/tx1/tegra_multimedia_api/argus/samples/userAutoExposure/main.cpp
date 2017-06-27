/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Error.h"
#include <stdio.h>
#include <stdlib.h>
#include <Argus/Argus.h>
#include <Argus/Ext/BayerAverageMap.h>
#include <EGLStream/EGLStream.h>
#include "PreviewConsumer.h"
#include <algorithm>
#include <math.h>

#define EXIT_IF_TRUE(val,msg)   \
        {if ((val)) {printf("%s\n",msg); return EXIT_FAILURE;}}
#define EXIT_IF_NULL(val,msg)   \
        {if (!val) {printf("%s\n",msg); return EXIT_FAILURE;}}
#define EXIT_IF_NOT_OK(val,msg) \
        {if (val!=Argus::STATUS_OK) {printf("%s\n",msg); return EXIT_FAILURE;}}

enum AnalysisMode {
    NO_ANALYSIS_MODE,
    BAYER_HISTOGRAM_ANALYSIS_MODE,
    BAYER_AVERAGE_MAP_ANALYSIS_MODE
};


using namespace Argus;

namespace ArgusSamples
{

// Globals and derived constants.
EGLDisplayHolder g_display;

const float TARGET_EXPOSURE_LEVEL = 0.18f;
const float TARGET_EXPOSURE_LEVEL_RANGE_TOLERANCE = 0.01f;
const float BAYER_CLIP_COUNT_MAX = 0.10f;
const float CENTER_WEIGHTED_DISTANCE = 10.0f;
const float CENTER_WEIGHT = 50.0f;
const int CAPTURE_COUNT = 500;

/*
 * Program: userAutoExposure
 * Function: To display preview images to the device display to illustrate a Bayer
 *           Histogram based exposure time manager technique in real time
 */
static bool execute(AnalysisMode analysisMode)
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    if (analysisMode == NO_ANALYSIS_MODE)
    {
        printf("Usage: argus_userautoexposure <option>\n\n");
        printf("where <option> can be one of the following:\n\n");
        printf("     -h     Uses BayerHistogram for analysis\n");
        printf("            This is the default analysis method\n");
        printf("     -a     Uses BayerAverageMap for analysis\n");
        printf("     -?     Displays this options list\n\n");
        return false;
    }
    /*
     * Set up Argus API Framework, identify available camera devices, create
     * a capture session for the first available device, and set up the event
     * queue for completed events
     */

    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    EXIT_IF_NULL(iCameraProvider, "Cannot get core camera provider interface");

    std::vector<CameraDevice*> cameraDevices;
    EXIT_IF_NOT_OK(iCameraProvider->getCameraDevices(&cameraDevices),
        "Failed to get camera devices");
    EXIT_IF_NULL(cameraDevices.size(), "No camera devices available");

    UniqueObj<CaptureSession> captureSession(
        iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iSession = interface_cast<ICaptureSession>(captureSession);
    EXIT_IF_NULL(iSession, "Cannot get Capture Session Interface");

    IEventProvider *iEventProvider = interface_cast<IEventProvider>(captureSession);
    EXIT_IF_NULL(iEventProvider, "iEventProvider is NULL");

    std::vector<EventType> eventTypes;
    eventTypes.push_back(EVENT_TYPE_CAPTURE_COMPLETE);
    UniqueObj<EventQueue> queue(iEventProvider->createEventQueue(eventTypes));
    IEventQueue *iQueue = interface_cast<IEventQueue>(queue);
    EXIT_IF_NULL(iQueue, "event queue interface is NULL");

    /*
     * Creates the stream between the Argus camera image capturing
     * sub-system (producer) and the image acquisition code (consumer)
     * preview thread.  A consumer object is created from the stream
     * to be used to request the image frame.  A successfully submitted
     * capture request activates the stream's functionality to eventually
     * make a frame available for acquisition, in the preview thread,
     * and then display it on the device screen.
     */

    UniqueObj<OutputStreamSettings> streamSettings(iSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings =
        interface_cast<IOutputStreamSettings>(streamSettings);
    EXIT_IF_NULL(iStreamSettings, "Cannot get OutputStreamSettings Interface");

    iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iStreamSettings->setResolution(Size(640,480));
    iStreamSettings->setEGLDisplay(g_display.get());

    UniqueObj<OutputStream> stream(iSession->createOutputStream(streamSettings.get()));
    IStream *iStream = interface_cast<IStream>(stream);
    EXIT_IF_NULL(iStream, "Cannot get OutputStream Interface");

    PreviewConsumerThread previewConsumerThread(
        iStream->getEGLDisplay(), iStream->getEGLStream());
    PROPAGATE_ERROR(previewConsumerThread.initialize());
    PROPAGATE_ERROR(previewConsumerThread.waitRunning());

    UniqueObj<Request> request(iSession->createRequest(CAPTURE_INTENT_MANUAL));
    IRequest *iRequest = interface_cast<IRequest>(request);
    EXIT_IF_NULL(iRequest, "Failed to get capture request interface");

    Argus::ISourceSettings *iSourceSettings =
        Argus::interface_cast<Argus::ISourceSettings>(iRequest->getSourceSettings());
    EXIT_IF_NULL(iSourceSettings, "Failed to get source settings interface");

    ISensorMode *iSensorMode = interface_cast<ISensorMode>(iSourceSettings->getSensorMode());
    EXIT_IF_NULL(iSensorMode, "Failed to get sensor mode interface");

    Range<uint64_t> limitExposureTimeRange = iSensorMode->getExposureTimeRange();
    printf("Sensor Exposure Range min %ju, max %ju\n",
        limitExposureTimeRange.min, limitExposureTimeRange.max);

    Size sensorResolution = iSensorMode->getResolution();

    // This line allows for testing where Analog Gain will be decreased
    // when a light is pointed into the camera device.  Otherwise, the
    // Exposure Time is always able to compensate for changes and analog
    // gain would never be decreased.
    uint64_t FIFTH_OF_A_SECOND = 20000000;
    limitExposureTimeRange.min = FIFTH_OF_A_SECOND;

    EXIT_IF_NOT_OK(iRequest->enableOutputStream(stream.get()),
        "Failed to enable stream in capture request");

    const uint64_t THIRD_OF_A_SECOND = 33333333;
    EXIT_IF_NOT_OK(iSourceSettings->setExposureTimeRange(Range<uint64_t>(THIRD_OF_A_SECOND)),
        "Unable to set the Source Settings Exposure Time Range");

    Range<float> sensorModeAnalogGainRange = iSensorMode->getAnalogGainRange();
    printf("Sensor Analog Gain range min %f, max %f\n",
        sensorModeAnalogGainRange.min, sensorModeAnalogGainRange.max);

    EXIT_IF_NOT_OK(iSourceSettings->setGainRange(Range<float>(sensorModeAnalogGainRange.min)),
        "Unable to set the Source Settings Gain Range");

    if (analysisMode == BAYER_AVERAGE_MAP_ANALYSIS_MODE)
    {
        // Enable BayerAverageMap generation in the request.
        Ext::IBayerAverageMapSettings *iBayerAverageMapSettings =
            interface_cast<Ext::IBayerAverageMapSettings>(request);
        EXIT_IF_NULL(iBayerAverageMapSettings,
            "Failed to get BayerAverageMapSettings interface");
        iBayerAverageMapSettings->setBayerAverageMapEnable(true);
    }

    EXIT_IF_NOT_OK(iSession->repeat(request.get()), "Unable to submit repeat() request");

    // Set tolerance difference from the target before making adjustments.
    const Range<float> targetRange(TARGET_EXPOSURE_LEVEL - TARGET_EXPOSURE_LEVEL_RANGE_TOLERANCE,
                                   TARGET_EXPOSURE_LEVEL + TARGET_EXPOSURE_LEVEL_RANGE_TOLERANCE);
    printf("Exposure target range is from %f to %f\n", targetRange.min, targetRange.max);

    for (int frameCaptureLoop = 0; frameCaptureLoop < CAPTURE_COUNT; frameCaptureLoop++)
    {
        // Keep PREVIEW display window serviced
        window.pollEvents();

        const uint64_t ONE_SECOND = 1000000000;
        iEventProvider->waitForEvents(queue.get(), ONE_SECOND);
        EXIT_IF_TRUE(iQueue->getSize() == 0, "No events in queue");

        const Event* event = iQueue->getEvent(iQueue->getSize() - 1);
        const IEventCaptureComplete *iEventCaptureComplete
            = interface_cast<const IEventCaptureComplete>(event);
        EXIT_IF_NULL(iEventCaptureComplete, "Failed to get EventCaptureComplete Interface");

        const CaptureMetadata *metaData = iEventCaptureComplete->getMetadata();
        const ICaptureMetadata* iMetadata = interface_cast<const ICaptureMetadata>(metaData);
        EXIT_IF_NULL(iMetadata, "Failed to get CaptureMetadata Interface");

        uint64_t frameExposureTime = iMetadata->getSensorExposureTime();
        float frameGain = iMetadata->getSensorAnalogGain();

        printf("Frame metadata ExposureTime %ju, Analog Gain %f\n",
            frameExposureTime, frameGain);

        float curExposureLevel;

        if (analysisMode == BAYER_HISTOGRAM_ANALYSIS_MODE)
        {
            /*
             * By using the Bayer Histogram, find the exposure middle point
             * in the histogram to be used as a guide as to whether to increase
             * or decrease the Exposure Time or Analog Gain
             */

            const IBayerHistogram* bayerHistogram =
                interface_cast<const IBayerHistogram>(iMetadata->getBayerHistogram());
            EXIT_IF_NULL(bayerHistogram, "Unable to get Bayer Histogram from metadata");

            uint64_t binCount = (uint64_t) bayerHistogram->getBinCount();

            uint64_t halfPixelTotal = sensorResolution.width * sensorResolution.height *
                BAYER_CHANNEL_COUNT / 2;
            uint64_t sum = 0;
            uint64_t currentBin;
            for (currentBin = 0; currentBin < binCount; currentBin++)
            {
                for (uint64_t channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
                {
                    sum += bayerHistogram->getBinData((BayerChannel)channel, currentBin);
                }
                if (sum > halfPixelTotal)
                {
                    break;
                }
            }
            curExposureLevel = (float) ++currentBin / (float)binCount;
        }
        else
        {
            /*
             * By using the Bayer Average Map, find the average point in the
             * region to be used as a guide as to whether to increase or
             * decrease the Exposure Time or Analog Gain
             */

            const Ext::IBayerAverageMap* iBayerAverageMap =
                interface_cast<const Ext::IBayerAverageMap>(metaData);
            EXIT_IF_NULL(iBayerAverageMap, "Failed to get IBayerAverageMap interface");
            Array2D< BayerTuple<float> > averages;
            EXIT_IF_NOT_OK(iBayerAverageMap->getAverages(&averages),
                "Failed to get averages");
            Array2D< BayerTuple<uint32_t> > clipCounts;
            EXIT_IF_NOT_OK(iBayerAverageMap->getClipCounts(&clipCounts),
                "Failed to get clip counts");
            uint32_t pixelsPerBinPerChannel = iBayerAverageMap->getBinSize().width *
                                              iBayerAverageMap->getBinSize().height /
                                              BAYER_CHANNEL_COUNT;
            float centerX = (float)(averages.size().width-1) / 2;
            float centerY = (float)(averages.size().height-1) / 2;

            // Using the BayerAverageMap, get the average instensity across the checked area
            float weightedPixelCount = 0.0f;
            float weightedAverageTotal = 0.0f;
            for (uint32_t x = 0; x < averages.size().width; x++)
            {
                for (uint32_t y = 0; y < averages.size().height; y++)
                {
                    /*
                     * Bin averages disregard pixels which are the result
                     * of intensity clipping.  A total of these over exposed
                     * pixels is kept and retrieved by clipCounts(x,y).  The
                     * following formula treats these pixels as a max intensity
                     * of 1.0f and adjusts the overall average upwards depending
                     * on the number of them.
                     *
                     * Also, only the GREEN EVEN channel is used to determine
                     * the exposure level, as GREEN is usually used for luminance
                     * evaluation, and the two GREEN channels tend to be equivalent
                     */

                    float clipAdjustedAverage =
                        (averages(x, y).gEven() *
                            (pixelsPerBinPerChannel - clipCounts(x, y).gEven()) +
                        clipCounts(x, y).gEven()) / pixelsPerBinPerChannel;

                    /*
                     * This will add the flat averages across the entire image
                     */
                    weightedAverageTotal += clipAdjustedAverage;
                    weightedPixelCount += 1.0f;

                    /*
                     * This will add more 'weight' or significance to averages closer to
                     * the center of the image
                     */
                    float distance = sqrt((pow(fabs((float)x-centerX),2)) +
                                          (pow(fabs((float)y-centerY),2)));
                    if (distance < CENTER_WEIGHTED_DISTANCE)
                    {
                        weightedAverageTotal += clipAdjustedAverage *
                            (1.0f - pow(distance/CENTER_WEIGHTED_DISTANCE, 2)) * CENTER_WEIGHT;
                        weightedPixelCount +=
                            (1.0f - pow(distance/CENTER_WEIGHTED_DISTANCE, 2)) * CENTER_WEIGHT;
                    }
                }
            }
            if (weightedPixelCount)
            {
                curExposureLevel = weightedAverageTotal / weightedPixelCount;
            }
            else
            {
                curExposureLevel = 1.0f;
            }
        }

        /*
         * If the acquired target is outside the target range, then
         * calculate adjustment values for the Exposure Time and/or the Analog Gain to bring
         * the next target into range.
         */

        printf("Exposure level at %0.3f, ", curExposureLevel);
        if (curExposureLevel > targetRange.max)
        {
            if (frameGain > sensorModeAnalogGainRange.min &&
                frameExposureTime <= limitExposureTimeRange.min)
            {
                frameGain = std::max(frameGain *
                     TARGET_EXPOSURE_LEVEL / curExposureLevel,
                     sensorModeAnalogGainRange.min);
                printf("decreasing analog gain to %f\n", frameGain);
                EXIT_IF_NOT_OK(iSourceSettings->setGainRange(Range<float>(frameGain)),
                    "Unable to set the Source Settings Gain Range");
            }
            else
            {
                frameExposureTime = std::max(limitExposureTimeRange.min,
                    (uint64_t) ((float)frameExposureTime *
                        TARGET_EXPOSURE_LEVEL / curExposureLevel));
                printf("decreasing Exposure Time to %ju\n", frameExposureTime);
                EXIT_IF_NOT_OK(iSourceSettings->setExposureTimeRange(
                    Range<uint64_t>(frameExposureTime)),
                    "Unable to set the Source Settings Exposure Time Range");
            }
        }
        else if (curExposureLevel < targetRange.min)
        {
            if (frameGain < sensorModeAnalogGainRange.max)
            {
                frameGain = std::min(frameGain *
                    TARGET_EXPOSURE_LEVEL / curExposureLevel,
                    sensorModeAnalogGainRange.max);
                printf("increasing analog gain to %f\n", frameGain);
                EXIT_IF_NOT_OK(iSourceSettings->setGainRange(Range<float>(frameGain)),
                    "Unable to set the Source Settings Gain Range");
            }
            else
            {
                frameExposureTime = std::min(limitExposureTimeRange.max,
                    (uint64_t) ((float)frameExposureTime *
                        TARGET_EXPOSURE_LEVEL / curExposureLevel));
                printf("increasing Exposure Time to %ju\n", frameExposureTime);
                EXIT_IF_NOT_OK(iSourceSettings->setExposureTimeRange(
                    Range<uint64_t>(frameExposureTime)),
                    "Unable to set the Source Settings Exposure Time Range");
            }
        }
        else
        {
            printf("currently within target range\n");
            continue;
        }
        /*
         * The modified request is re-submitted to terminate the previous repeat() with
         * the old settings and begin captures with the new settings
         */
        iSession->repeat(request.get());
    }

    iSession->stopRepeat();
    iSession->waitForIdle();

    // Destroy the output streams (stops consumer threads).
    stream.reset();

    // Wait for the consumer threads to complete.
    PROPAGATE_ERROR(previewConsumerThread.shutdown());

    // Shut down Argus.
    cameraProvider.reset();

    return EXIT_SUCCESS;
}

}; // namespace ArgusSamples

int main(int argc, const char *argv[])
{
    AnalysisMode analysisMode = NO_ANALYSIS_MODE;

    if (argc == 1)
    {
        analysisMode = BAYER_HISTOGRAM_ANALYSIS_MODE;
    }
    else if (argc == 2)
    {
        if (strlen(argv[1]) != 2 || *argv[1] != '-' ||
            (*(argv[1]+1) != '?' && *(argv[1]+1) != 'h' && *(argv[1]+1) != 'a'))
        {
            printf("\nInvalid option: '%s'\n\n", argv[1]);
        }
        else if (*(argv[1]+1) == 'h')
        {
            analysisMode = BAYER_HISTOGRAM_ANALYSIS_MODE;
        }
        else if (*(argv[1]+1) == 'a')
        {
            analysisMode = BAYER_AVERAGE_MAP_ANALYSIS_MODE;
        }
    }
    if (!ArgusSamples::execute(analysisMode))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
