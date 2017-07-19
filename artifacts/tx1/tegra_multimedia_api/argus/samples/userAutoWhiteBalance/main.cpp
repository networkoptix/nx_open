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

const float BAYER_CLIP_COUNT_MAX = 0.25f;
const float BAYER_MISSING_SAMPLE_TOLERANCE = 0.10f;
const int CAPTURE_COUNT = 300;

/*
 * Program: userAutoWhiteBalance
 * Function: To display 101 preview images to the device display to illustrate a
 *           Grey World White Balance technique for adjusting the White Balance in real time
 *           through the use of setWbGains() and setAwbMode(AWB_MODE_MANUAL) calls.
 */

static bool execute(AnalysisMode analysisMode)
{
    float preceedingChannelAvg[BAYER_CHANNEL_COUNT] = {1.0f, 1.5f, 1.5f, 1.5f};

    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    if (analysisMode == NO_ANALYSIS_MODE)
    {
        printf("Usage: argus_userautowhitebalance <option>\n\n");
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

    UniqueObj<Request> request(iSession->createRequest(CAPTURE_INTENT_PREVIEW));
    IRequest *iRequest = interface_cast<IRequest>(request);
    EXIT_IF_NULL(iRequest, "Failed to get capture request interface");

    EXIT_IF_NOT_OK(iRequest->enableOutputStream(stream.get()),
        "Failed to enable stream in capture request");

    IAutoControlSettings* iAutoControlSettings =
        interface_cast<IAutoControlSettings>(iRequest->getAutoControlSettings());
    EXIT_IF_NULL(iAutoControlSettings, "Failed to get AutoControlSettings interface");

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

    /*
     * Using the image capture event metadata, acquire the bayer histogram and then compute
     * a weighted average for each channel.  Use these weighted averages to create a White
     * Balance Channel Gain array to use for setting the manual white balance of the next
     * capture request.
     */
    int frameCaptureLoop = 0;
    while (frameCaptureLoop < CAPTURE_COUNT)
    {
        // Keep PREVIEW display window serviced
        window.pollEvents();

        const uint64_t ONE_SECOND = 1000000000;
        iEventProvider->waitForEvents(queue.get(), ONE_SECOND);
        EXIT_IF_TRUE(iQueue->getSize() == 0, "No events in queue");

        frameCaptureLoop += iQueue->getSize();

        const Event* event = iQueue->getEvent(iQueue->getSize() - 1);
        const IEventCaptureComplete *iEventCaptureComplete
            = interface_cast<const IEventCaptureComplete>(event);
        EXIT_IF_NULL(iEventCaptureComplete, "Failed to get EventCaptureComplete Interface");

        const CaptureMetadata *metaData = iEventCaptureComplete->getMetadata();
        const ICaptureMetadata* iMetadata = interface_cast<const ICaptureMetadata>(metaData);
        EXIT_IF_NULL(iMetadata, "Failed to get CaptureMetadata Interface");

        float channelTotals[BAYER_CHANNEL_COUNT];
        float channelAvg[BAYER_CHANNEL_COUNT];
        if (analysisMode == BAYER_HISTOGRAM_ANALYSIS_MODE)
        {
            const IBayerHistogram* bayerHistogram =
                interface_cast<const IBayerHistogram>(iMetadata->getBayerHistogram());

            uint32_t binCount = bayerHistogram->getBinCount();
            for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
            {
                channelTotals[channel] = 0;
                channelAvg[channel] = 0;
                for (uint bin = 0; bin < binCount; bin++)
                {
                    float binData = bayerHistogram->getBinData((BayerChannel)channel, bin);
                    channelTotals[channel] += binData;
                    channelAvg[channel] += binData*(float)bin;
                }
                if (channelTotals[channel])
                {
                    channelAvg[channel] /= channelTotals[channel];
                }
            }
            printf("Histogram ");
        }
        else
        {
            const Ext::IBayerAverageMap* iBayerAverageMap =
                interface_cast<const Ext::IBayerAverageMap>(metaData);
            EXIT_IF_NULL(iBayerAverageMap, "Failed to get IBayerAverageMap interface");
            Array2D< BayerTuple<float> > averages;
            EXIT_IF_NOT_OK(iBayerAverageMap->getAverages(&averages),
                "Failed to get averages");
            Array2D< BayerTuple<uint32_t> > clipCounts;
            EXIT_IF_NOT_OK(iBayerAverageMap->getClipCounts(&clipCounts),
                "Failed to get clip counts");
            uint32_t pixelsPerBinPerChannel =
                iBayerAverageMap->getBinSize().width *
                iBayerAverageMap->getBinSize().height /
                BAYER_CHANNEL_COUNT;

            memset(channelTotals, 0, BAYER_CHANNEL_COUNT * sizeof(float));
            memset(channelAvg, 0, BAYER_CHANNEL_COUNT * sizeof(float));

            // Using the BayerAverageMap, get the average instensity across the checked area
            uint32_t usedBins = 0;
            for (uint32_t x = 0; x < averages.size().width; x++)
            {
                for (uint32_t y = 0; y < averages.size().height; y++)
                {
                    /*
                     * Bins with excessively high clip counts many contain
                     * misleading averages, since clipped pixels are ignored
                     * in the averages, and so these bins are ignored.
                     *
                     */
                    if ((float)clipCounts(x, y).r() / pixelsPerBinPerChannel
                            <= BAYER_CLIP_COUNT_MAX &&
                        (float)clipCounts(x, y).gEven() / pixelsPerBinPerChannel
                            <= BAYER_CLIP_COUNT_MAX &&
                        (float)clipCounts(x, y).gOdd() / pixelsPerBinPerChannel
                            <= BAYER_CLIP_COUNT_MAX &&
                        (float)clipCounts(x, y).b() / pixelsPerBinPerChannel
                            <= BAYER_CLIP_COUNT_MAX)
                    {
                        channelTotals[0] += averages(x, y).r();
                        channelTotals[1] += averages(x, y).gOdd();
                        channelTotals[2] += averages(x, y).gEven();
                        channelTotals[3] += averages(x, y).b();
                        usedBins++;
                    }
                }
            }
            /*
             * This check is to determine if enough BayerAverageMap samples
             * have been collected to give a valid enough evaluation of the
             * white balance adjustment.  If not enough samples, then the
             * most previous valid results are used.  If valid, then the valid
             * set is stored for future use.
             */
            if ((float)usedBins / (averages.size().width * averages.size().height) <
                ( 1.0f - BAYER_MISSING_SAMPLE_TOLERANCE ))
            {
                printf("Clipped sensor sample percentage of %0.1f%%"
                    " exceed tolerance of %0.1f%%\n"
                    "Using last valid BayerAverageMap intensity values\n",
                    (1.0f - (float)usedBins /
                        (averages.size().width * averages.size().height)) * 100,
                    BAYER_MISSING_SAMPLE_TOLERANCE * 100);
                memcpy(channelAvg, preceedingChannelAvg, sizeof(channelAvg));
            }
            else
            {
                for (int channel=0; channel<BAYER_CHANNEL_COUNT; channel++)
                {
                    channelAvg[channel] = channelTotals[channel] / usedBins;
                    preceedingChannelAvg[channel] = channelAvg[channel];
                }
            }
            printf("BayerAverageMap ");
        }

        float maxGain = 0;
        for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
        {
            if (channelAvg[channel] > maxGain)
            {
                maxGain = channelAvg[channel];
            }
        }
        float whiteBalanceGains[BAYER_CHANNEL_COUNT];
        for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
        {
            whiteBalanceGains[channel] = maxGain / channelAvg[channel];
        }

        printf("Intensity: r:%0.3f gO:%0.3f gE:%0.3f b:%0.3f   "
               "WBGains: r:%0.3f gO:%0.3f gE:%0.3f b:%0.3f\n",
               channelAvg[0],
               channelAvg[1],
               channelAvg[2],
               channelAvg[3],
               whiteBalanceGains[0],
               whiteBalanceGains[1],
               whiteBalanceGains[2],
               whiteBalanceGains[3]);

        BayerTuple<float> bayerGains(whiteBalanceGains[0], whiteBalanceGains[1],
                                     whiteBalanceGains[2], whiteBalanceGains[3]);

        iAutoControlSettings->setWbGains(bayerGains);
        iAutoControlSettings->setAwbMode(AWB_MODE_MANUAL);

        EXIT_IF_NOT_OK(iSession->repeat(request.get()), "Unable to submit repeat() request");
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
