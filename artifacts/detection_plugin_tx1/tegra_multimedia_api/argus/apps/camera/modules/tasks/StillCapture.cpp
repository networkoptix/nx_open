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

#include <sstream>
#include <iomanip>
#include <unistd.h>

#include "StillCapture.h"
#include "Renderer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "EventThread.h"

namespace ArgusSamples
{

TaskStillCapture::TaskStillCapture()
    : m_initialized(false)
    , m_running(false)
    , m_wasRunning(false)
    , m_captureIndex(0)
    , m_eventThread(NULL)
{
}

TaskStillCapture::~TaskStillCapture()
{
    shutdown();
}

bool TaskStillCapture::initialize()
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(Dispatcher::getInstance().m_deviceOpen.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::onDeviceOpenChanged)));

    m_initialized = true;

    return true;
}

bool TaskStillCapture::onDeviceOpenChanged(const Observed &source)
{
    const bool isOpen = static_cast<const Value<bool>&>(source).get();

    // If the current device is closed the request needs to be recreated on the new device. Stop
    // and then start when the device is opened again.
    if (!isOpen)
    {
        m_wasRunning = m_running;
        PROPAGATE_ERROR(stop());
    }
    else if (m_wasRunning)
    {
        m_wasRunning = false;
        PROPAGATE_ERROR(start());
    }

    return true;
}

bool TaskStillCapture::start()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (m_running)
        return true;

    m_perfTracker.onEvent(TASK_START);
    Dispatcher &dispatcher = Dispatcher::getInstance();
    Renderer &renderer = Renderer::getInstance();

    PROPAGATE_ERROR(dispatcher.createRequest(m_previewRequest, Argus::CAPTURE_INTENT_PREVIEW));

    // Create the preview stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_previewRequest.get(), m_previewStream));

    Argus::IStream *iStream =
        Argus::interface_cast<Argus::IStream>(m_previewStream);
    if (!iStream)
        ORIGINATE_ERROR("Failed to get IStream interface");

    // render the preview stream
    PROPAGATE_ERROR(renderer.bindStream(iStream->getEGLStream()));

    const Argus::Size streamSize = iStream->getResolution();
    PROPAGATE_ERROR(renderer.setStreamAspectRatio(iStream->getEGLStream(),
        (float)streamSize.width / (float)streamSize.height));
    PROPAGATE_ERROR(renderer.setStreamActive(iStream->getEGLStream(), true));

    // Enable the preview stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(m_previewRequest.get(), m_previewStream.get()));

    std::vector<Argus::EventType> eventTypes;
    eventTypes.push_back(Argus::EVENT_TYPE_CAPTURE_COMPLETE);

    Argus::UniqueObj<Argus::EventQueue> eventQueue;
    PROPAGATE_ERROR(dispatcher.createEventQueue(eventTypes, eventQueue));

    // pass ownership of eventQueue to EventThread
    m_eventThread.reset(new EventThread(NULL, eventQueue.release(), &m_perfTracker));
    if (!m_eventThread)
    {
        ORIGINATE_ERROR("Failed to allocate EventThread");
    }

    PROPAGATE_ERROR(m_eventThread->initialize());
    PROPAGATE_ERROR(m_eventThread->waitRunning());

    m_perfTracker.onEvent(ISSUE_CAPTURE);
    // start the repeating request for the preview
    PROPAGATE_ERROR(dispatcher.startRepeat(m_previewRequest.get()));

    m_running = true;

    return true;
}

bool TaskStillCapture::stop()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        return true;

    m_perfTracker.onEvent(CLOSE_REQUESTED);

    PROPAGATE_ERROR(m_eventThread->shutdown());
    m_eventThread.reset();

    Dispatcher &dispatcher = Dispatcher::getInstance();
    Renderer &renderer = Renderer::getInstance();

    // stop the repeating request
    PROPAGATE_ERROR(dispatcher.stopRepeat());

    PROPAGATE_ERROR(renderer.setStreamActive(
        Argus::interface_cast<Argus::IStream>(m_previewStream)->getEGLStream(), false));

    PROPAGATE_ERROR(dispatcher.waitForIdle());
    m_perfTracker.onEvent(FLUSH_DONE);

    // destroy the preview producer
    PROPAGATE_ERROR(dispatcher.disableOutputStream(m_previewRequest.get(), m_previewStream.get()));
    const EGLStreamKHR eglStream =
        Argus::interface_cast<Argus::IStream>(m_previewStream)->getEGLStream();

    m_previewStream.reset();

    // unbind the preview stream from the renderer
    PROPAGATE_ERROR(renderer.unbindStream(eglStream));

    // destroy the preview request
    PROPAGATE_ERROR(dispatcher.destroyRequest(m_previewRequest));

    m_perfTracker.onEvent(CLOSE_DONE);

    m_running = false;

    return true;
}

bool TaskStillCapture::execute()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        ORIGINATE_ERROR("Not running");

    Dispatcher &dispatcher = Dispatcher::getInstance();

    Argus::UniqueObj<Argus::Request> stillRequest;
    PROPAGATE_ERROR(dispatcher.createRequest(stillRequest, Argus::CAPTURE_INTENT_STILL_CAPTURE));

    // Create the still stream
    Argus::UniqueObj<Argus::OutputStream> stillStream;
    PROPAGATE_ERROR(dispatcher.createOutputStream(stillRequest.get(), stillStream));

    // Enable the still stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(stillRequest.get(), stillStream.get()));

    // Create the frame consumer
    Argus::UniqueObj<EGLStream::FrameConsumer> consumer(
        EGLStream::FrameConsumer::create(stillStream.get()));
    EGLStream::IFrameConsumer *iFrameConsumer =
        Argus::interface_cast<EGLStream::IFrameConsumer>(consumer);
    if (!iFrameConsumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    // do the capture
    PROPAGATE_ERROR(dispatcher.capture(stillRequest.get()));

    // aquire the frame
    Argus::UniqueObj<EGLStream::Frame> frame(iFrameConsumer->acquireFrame());
    if (!frame)
        ORIGINATE_ERROR("Failed to aquire frame");

    // Use the IFrame interface to provide access to the Image in the Frame.
    EGLStream::IFrame *iFrame = Argus::interface_cast<EGLStream::IFrame>(frame);
    if (!iFrame)
        ORIGINATE_ERROR("Failed to get IFrame interface.");

    EGLStream::Image *image = iFrame->getImage();
    if (!image)
        ORIGINATE_ERROR("Failed to get image.");

    // Get the JPEG interface.
    EGLStream::IImageJPEG *iJPEG =
        Argus::interface_cast<EGLStream::IImageJPEG>(image);
    if (!iJPEG)
        ORIGINATE_ERROR("Failed to get IImageJPEG interface.");

    // build the file name
    std::ostringstream fileName;
    fileName << dispatcher.m_outputPath.get();
    if (dispatcher.m_outputPath.get() != "/dev/null")
        fileName << "/image" << std::setfill('0') << std::setw(4) << m_captureIndex << ".jpg";

    // Write a JPEG to disk.
    if (iJPEG->writeJPEG(fileName.str().c_str()) == Argus::STATUS_OK)
    {
        PROPAGATE_ERROR(dispatcher.message("Captured a still image to '%s'\n",
            fileName.str().c_str()));
    }
    else
    {
        ORIGINATE_ERROR("Failed to write JPEG to '%s'\n", fileName.str().c_str());
    }

    ++m_captureIndex;

    // release the frame.
    frame.reset();

    // destroy the still stream
    PROPAGATE_ERROR(dispatcher.disableOutputStream(stillRequest.get(), stillStream.get()));
    stillStream.reset();

    // destroy the still request
    PROPAGATE_ERROR(dispatcher.destroyRequest(stillRequest));

    // destroy the still consumer
    consumer.reset();

    return true;
}

bool TaskStillCapture::shutdown()
{
    if (!m_initialized)
        return true;

    // stop the module
    PROPAGATE_ERROR(stop());

    PROPAGATE_ERROR_CONTINUE(Dispatcher::getInstance().m_deviceOpen.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskStillCapture::onDeviceOpenChanged)));

    m_initialized = false;

    return true;
}

}; // namespace ArgusSamples
