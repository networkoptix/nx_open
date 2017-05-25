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

/**
 * @file NvV4l2Element.h
 *
 * @brief Helper class for V4L2 based components.
 */
#ifndef __NV_V4L2_ELEMENT_H__
#define __NV_V4L2_ELEMENT_H__

#include "NvElement.h"
#include "NvV4l2ElementPlane.h"

#include "v4l2_nv_extensions.h"

/**
 * @brief Helper class for V4L2 based components.
 *
 * This class provides common functionality for V4L2 components. V4L2 based
 * components like encoder/decoder extend from this class.
 *
 * This class has been modelled on v4l2 M2M devices. It includes FD of the device
 * opened using \v4l2_open, two planes (NvV4l2ElementPlane) viz. Output plane,
 * Capture plane and other helper functions such as setting/getting controls,
 * subscribing/dequeueing events, etc.
 */
class NvV4l2Element:public NvElement
{
public:
    virtual ~NvV4l2Element();

    /**
     * Subscibe to an V4L2 Event.
     *
     * Calls VIDIOC_SUBSCRIBE_EVENT ioctl internally
     *
     * @param[in] type Type of the event
     * @param[in] id ID of the event source
     * @param[in] flags Event flags
     * @returns 0 for success, -1 for failure
     */
    int subscribeEvent(uint32_t type, uint32_t id, uint32_t flags);
    /**
     * Dequeue a event from the element
     *
     * Calls VIDIOC_DQEVENT ioctl internally. User can specify the maximum time
     * to wait for dequeuing the event. The call blocks till an event is
     * dequeued successfully or timeout is reached.
     *
     * @param[in,out] event Reference to v4l2_event structure to be filled
     * @param[in] max_wait_ms Max time to wait for dequeuing an event in
     *                        in milliseconds
     * @returns 0 for success, -1 for failure
     */
    int dqEvent(struct v4l2_event &event, uint32_t max_wait_ms);

    /**
     * Set the value of a control
     *
     * Calls VIDIOC_S_CTRL ioctl internally
     *
     * @param[in] id ID of the control to be set
     * @param[in] value Value to be set on the control
     * @returns 0 for success, -1 for failure
     */
    int setControl(uint32_t id, int32_t value);
    /**
     * Get the value of a control
     *
     * Calls VIDIOC_G_CTRL ioctl internally
     *
     * @param[in] id ID of the control to get
     * @param[out] value Reference to the variable into which the control value
                         will be read
     * @returns 0 for success, -1 for failure
     */
    int getControl(uint32_t id, int32_t &value);

    /**
     * Set the value of several controls
     *
     * Calls VIDIOC_S_EXT_CTRLS ioctl internally
     *
     * @param[in] ctl Contains controls to be set
     * @returns 0 for success, -1 for failure
     */
    int setExtControls(struct v4l2_ext_controls &ctl);
    /**
     * Get the value of several controls
     *
     * Calls VIDIOC_G_EXT_CTRLS ioctl internally
     *
     * @param[in,out] ctl Contains controls to get
     * @returns 0 for success, -1 for failure
     */
    int getExtControls(struct v4l2_ext_controls &ctl);

    virtual int isInError();

    NvV4l2ElementPlane output_plane;  /**< Output plane of the element */
    NvV4l2ElementPlane capture_plane; /**< Capture plane of the element */

    /**
     *
     * Abort processing of queued buffers immediately. All the buffers are
     * returned to the application.
     *
     * Calls VIDIOC_STREAMOFF ioctl on the both the planes internally.
     *
     * @returns 0 for success, -1 for failure
     */
    int abort();

    /**
     * Wait till the element has processed all the output plane buffers
     *
     * Classes extending V4l2Element should implement this since the idle
     * condition will be component specific.
     *
     * @param[in] max_wait_ms Max time to wait in milliseconds
     * @returns 0 for success, -1 for failure
     */
    virtual int waitForIdle(uint32_t max_wait_ms);

    void *app_data; /**< Pointer to application specific data */

    /**
     * Enable profiling for the V4l2Element.
     *
     * Should be called before setting either plane formats.
     */
    void enableProfiling();

protected:
    int fd;     /**< FD of the device opened using \v4l2_open */

    uint32_t output_plane_pixfmt;  /**< Pixel format of output plane buffers */
    uint32_t capture_plane_pixfmt; /**< Pixel format of capture plane buffers */

    /**
     * Creates a new V4l2Element named \a name.
     *
     * This constructor calls v4l2_open on the \a dev_node. Sets an error if
     * v4l2_open fails.
     *
     * This function also checks if the device supports V4L2_CAP_VIDEO_M2M_MPLANE
     * capability.
     *
     * @param[in] comp_name Unique name to identity the element instance
     * @param[in] dev_node /dev/ * node of the device
     * @param[in] flags Flags to open the device with
     * @param[in] fields Profiler fields which are valid for the element.
     */
    NvV4l2Element(const char *comp_name, const char *dev_node, int flags, NvElementProfiler::ProfilerField fields);
};

#endif
