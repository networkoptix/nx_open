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
 * @file NvV4l2ElementPlane.h
 *
 * @brief Helper Class for operations performed on a V4L2 Element plane
 */

#ifndef __NV_V4L2_ELELMENT_PLANE_H__
#define __NV_V4L2_ELELMENT_PLANE_H__

#include <pthread.h>
#include "NvElement.h"
#include "NvLogging.h"
#include "NvBuffer.h"

/**
 * Prints a plane specific message of level #LOG_LEVEL_DEBUG.
 * Should not be used by applications.
 */
#define PLANE_DEBUG_MSG(str) COMP_DEBUG_MSG(plane_name << ":" << str);
/**
 * Prints a plane specific message of level #LOG_LEVEL_INFO.
 * Should not be used by applications.
 */
#define PLANE_INFO_MSG(str) COMP_INFO_MSG(plane_name << ":" << str);
/**
 * Prints a plane specific message of level #LOG_LEVEL_WARN.
 * Should not be used by applications.
 */
#define PLANE_WARN_MSG(str) COMP_WARN_MSG(plane_name << ":" << str);
/**
 * Prints a plane specific message of level #LOG_LEVEL_ERROR.
 * Should not be used by applications.
 */
#define PLANE_ERROR_MSG(str) COMP_ERROR_MSG(plane_name << ":" << str);
/**
 * Prints a plane specific system error message of level #LOG_LEVEL_ERROR.
 * Should not be used by applications.
 */
#define PLANE_SYS_ERROR_MSG(str) COMP_SYS_ERROR_MSG(plane_name << ":" << str);

/**
 * @brief Helper Class for operations performed on a V4L2 Element plane.
 *
 * This class is modelled on the planes of a V4L2 Element. It provides
 * convenient wrapper functions around V4L2 ioctls associated with plane
 * operations such as VIDIOC_G_FMT/VIDIOC_S_FMT, VIDIOC_REQBUFS,\
 * VIDIOC_STREAMON/VIDIOC_STREAMOFF, etc.
 *
 * The plane buffer type can be either V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE (for
 * the output plane) or V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE (for the capture
 * plane).
 *
 * The plane has an array of NvBuffer object pointers which is allocated and
 * initialized during #reqbufs call. These NvBuffer objects are similar to the
 * v4l2_buffer structures which are queued/dequeued.
 *
 * This class provides another feature useful for multi-threading. On calling
 * #startDQThread, it internally spawns a thread which runs infinitely till
 * signalled to stop. This thread keeps on trying to dequeue a buffer from the
 * plane and calls a #dqThreadCallback function specified by the user on
 * successful dequeue.
 *
 */
class NvV4l2ElementPlane
{

public:
    /**
     * Get the plane format
     *
     * Calls @b VIDIOC_G_FMT ioctl internally.
     *
     * @param[in,out] format Reference to v4l2_format structure to be filled
     * @returns 0 for success, -1 for failure
     */
    int getFormat(struct v4l2_format & format);
    /**
     * Set the plane format
     *
     * Calls @b VIDIOC_S_FMT ioctl internally.
     *
     * @param[in] format Reference to v4l2_format structure to be set on the plane
     * @returns 0 for success, -1 for failure
     */
    int setFormat(struct v4l2_format & format);

    /**
     * Get the cropping rectangle for the plane
     *
     * Calls @b VIDIOC_G_CROP ioctl internally.
     *
     * @param[in] crop Reference to v4l2_crop structure to be filled
     * @returns 0 for success, -1 for failure
     */
    int getCrop(struct v4l2_crop & crop);

    /**
     * Set the selection rectangle for the plane
     *
     * Calls @b VIDIOC_S_SELECTION ioctl internally.
     *
     * @param[in] target Rectangle selection type
     * @param[in] flags Flags to control selection adjustments
     * @param[in] rect Selection rectangle
     * @returns 0 for success, -1 for failure
     */
    int setSelection(uint32_t target, uint32_t flags, struct v4l2_rect & rect);

    /**
     * Request for buffers on the plane.
     *
     * Calls @b VIDIOC_REQBUFS ioctl internally. Creates an array of NvBuffer of
     * length equal to the count returned by the ioctl.
     *
     * @param[in] mem_type Type of V4L2 Memory to be requested
     * @param[in] num Number of buffers to request on the plane
     * @returns 0 for success, -1 for failure
     */
    int reqbufs(enum v4l2_memory mem_type, uint32_t num);
    /**
     * Query the status of buffer at index
     *
     * @warning This functions works only for V4L2_MEMORY_MMAP memory.
     *
     * Calls VIDIOC_QUERYBUF ioctl internally. Populates the \a length and \a
     * mem_offset members of all the NvBuffer::NvBufferPlane members of the
     * NvBuffer object at index \a buf_index.
     *
     * @param[in] buf_index Index of the buffer to query
     * @returns 0 for success, -1 for failure
     */
    int queryBuffer(uint32_t buf_index);
    /**
     * Export the buffer as DMABUF FD.
     *
     * @warning This functions works only for V4L2_MEMORY_MMAP memory.
     *
     * Calls VIDIOC_EXPBUF ioctl internally. Populates the \a fd member of all
     * the NvBuffer::NvBufferPlane members of NvBuffer object at index \a buf_in.
     *
     * @param[in] buf_index Index of the buffer to export
     * @returns 0 for success, -1 for failure
     */
    int exportBuffer(uint32_t buf_index);

    /**
     * Start or stop streaming on the plane.
     *
     * Calls VIDIOC_STREAMON/VIDIOC_STREAMOFF ioctls internally.
     *
     * @param[in] status Should be  true to start stream, false to stop stream
     * @returns 0 for success, -1 for failure
     */
    int setStreamStatus(bool status);

    /**
     * Check if the plane is streaming.
     *
     * @returns true if the plane is streaming, false otherwise.
     */
    bool getStreamStatus();

    /**
     * Set streaming parameters.
     *
     * Calls VIDIOC_S_PARM ioctl internally.
     *
     * @param[in] parm Reference to v4l2_streamparm structure to be set on the
     *                 plane
     * @returns 0 for success, -1 for failure
     */
    int setStreamParms(struct v4l2_streamparm & parm);

    /**
     * Helper function which encapsulates all the function calls required to
     * setup the plane for streaming.
     *
     * Calls #reqbufs internally. Then, for each of the buffers calls #queryBuffer,
     * #exportBuffer and maps the buffer/allocates the buffer memory depending
     * on the memory type.
     *
     * @sa deinitPlane
     *
     * @param[in] mem_type V4L2 Memory to use on the buffer
     * @param[in] num_buffers Number of buffer to request on the plane
     * @param[in] map boolean value indicating if the buffers should be mapped to
                      memory (Only for V4L2_MEMORY_MMAP)
     * @param[in] allocate boolean valued indicating if the buffers should be
                           allocated memory (Only for V4L2_MEMORY_USERPTR)
     * @returns 0 for success, -1 for failure
     */
    int setupPlane(enum v4l2_memory mem_type, uint32_t num_buffers, bool map, bool allocate);
    /**
     * Helper function which encapsulates all the function calls required to
     * deinitialize the plane for streaming.
     *
     * For each of the buffers unmaps/deallocates memory depending on the
     * memory type. Then, calls reqbufs with count zero.
     *
     * @sa setupPlane
     */
    void deinitPlane();

    /**
     * Get the streaming/buffer type of this plane.
     *
     * @returns Type of the buffer belonging to enum v4l2_buf_type
     */
    inline enum v4l2_buf_type getBufType()
    {
        return buf_type;
    }

    /**
     * Get the NvBuffer object at index n
     *
     * @returns NvBuffer object at index n, NULL if n >= Number of buffers
     */
    NvBuffer *getNthBuffer(uint32_t n);

    /**
     * Dequeue a buffer from the plane.
     *
     * This is a blocking call. This call returns when a buffer is successfully
     * dequeued or timeout is reached. If \a buffer is not NULL returns the
     * NvBuffer object at the index returned by the VIDIOC_DQBUF ioctl. If this
     * plane shares a buffer with other element and \a shared_buffer is not
     * NULL returns the shared NvBuffer object in \a shared_buffer.
     *
     * @param[in] v4l2_buf v4l2_buffer structure to be used for dequeueing
     * @param[out] buffer Returns the NvBuffer object associated with the dequeued
     *                    buffer. Can be NULL
     * @param[out] shared_buffer Returns the shared NvBuffer object, if the queued
     *             buffer is being shared with other elements. Can be NULL.
     * @param[in] num_retries Number of times to try dequeuing a buffer before
     *                        a failure is returned. In case of non-blocking
     *                        mode, this is equivalent to the number of
     *                        milliseconds to try to dequeue a buffer.
     * @returns 0 for success, -1 for failure/timeout
     */
    int dqBuffer(struct v4l2_buffer &v4l2_buf, NvBuffer ** buffer,
                 NvBuffer ** shared_buffer, uint32_t num_retries);
    /**
     * Queue a buffer on the plane.
     *
     * This function calls VIDIOC_QBUF internally. If this plane is sharing
     * buffer with other elements, the application can pass the pointer to the
     * shared NvBuffer object in \a shared_buffer.
     *
     * @param[in] v4l2_buf v4l2_buffer structure to be used for queueing
     * @param[in] shared_buffer Pointer to the shared NvBuffer object
     * @returns 0 for success, -1 for failure/timeout
     */
    int qBuffer(struct v4l2_buffer &v4l2_buf, NvBuffer * shared_buffer);

    /**
     * Get the number of buffers allocated/requested on the plane
     *
     * @returns Number of buffers
     */
    inline uint32_t getNumBuffers()
    {
        return num_buffers;
    }

    /**
     * Get the number of planes buffers on this plane contain for the currently
     * set format
     *
     * @returns Number of planes
     */
    inline uint32_t getNumPlanes()
    {
        return n_planes;
    }

    /**
     * Set the format of the planes of the buffer which will be used with this
     * plane.
     *
     * The buffer plane format needs to be set before calling #reqbufs, since
     * these are needed by the NvBuffer constructor.
     *
     * @sa reqbufs
     *
     * @param[in] n_planes Number of planes in the buffer
     * @param[in] planefmts Array of NvBufferPlaneFormat which describes the
     *            format of each of the plane. The array length should be at
     *            least @a n_planes.
     */
    void setBufferPlaneFormat(int n_planes, NvBuffer::NvBufferPlaneFormat * planefmts);

    /**
     * Get the number of buffers currently queued on the plane
     *
     * @returns number of buffers  currently queued on the plane
     */
    inline uint32_t getNumQueuedBuffers()
    {
        return num_queued_buffers;
    }

    /**
     * Get the total number of buffers dequeued from the plane
     *
     * @returns total number of buffers dequeued from the plane
     */
    inline uint32_t getTotalDequeuedBuffers()
    {
        return total_dequeued_buffers;
    }

    /**
     * Get the total number of buffers queued on the plane
     *
     * @returns total number of buffers queued on the plane
     */
    inline uint32_t getTotalQueuedBuffers()
    {
        return total_queued_buffers;
    }

    /**
     * Wait till all the buffers of the plane get queued
     *
     * This is a blocking call which returns when all the buffers get queued
     * or timeout is reached.

     * @param[in] max_wait_ms Maximum time to wait in milliseconds
     * @returns 0 for success, -1 for failure/timeout
     */
    int waitAllBuffersQueued(uint32_t max_wait_ms);
    /**
     * Wait till all the buffers of the plane get dequeued
     *
     * This is a blocking call which returns when all the buffers get dequeued
     * or timeout is reached.

     * @param[in] max_wait_ms Maximum time to wait in milliseconds
     * @returns 0 for success, -1 for failure/timeout
     */
    int waitAllBuffersDequeued(uint32_t max_wait_ms);

    /**
     * This is a callback function type which is called by the DQ Thread when
     * it successfully dequeues a buffer from the plane. Applications must implement
     * this and set the callback using #setDQThreadCallback.
     *
     * Setting the stream to off will automatically stop this thread.
     *
     * @sa setDQThreadCallback, #startDQThread
     *
     * @param v4l2_buf v4l2_buffer structure which was used for dequeueing
     * @param buffer NvBuffer object at the @a index contained in @a v4l2_buf
     * @param shared_buffer If the plane shares buffer with another elements
     *         pointer to the NvBuffer object else NULL
     * @param data Pointer to application specific data which was set with
     *             #startDQThread
     * @returns If the application implementing this calls returns false,
     *          the DQThread will be stopped else the DQThread will continue running.
     */
    typedef bool(*dqThreadCallback) (struct v4l2_buffer * v4l2_buf,
            NvBuffer * buffer, NvBuffer * shared_buffer,
            void *data);

    /**
     * Set the DQ Thread callback function.
     *
     * The callback function is called from the DQ Thread once a buffer is
     * successfully dequeued.

     * @param[in] callback Function to be called on succesful dequeue
     * @returns true for success, false for failure
     */
    bool setDQThreadCallback(dqThreadCallback callback);
    /**
     * Start DQ Thread.
     *
     * This function starts a thread internally. On successful dequeue of a
     * buffer from the plane, #dqThreadCallback function set using
     * setDQThreadCallback gets called.
     *
     * Setting the stream to off will automatically stop the thread.
     *
     * @sa stopDQThread, waitForDQThread
     *
     * @param[in] data Application data pointer. This is provided as an
     *                 argument in the dqThreadCallback function
     * @returns 0 for success, -1 for failure/timeout
     */
    int startDQThread(void *data);
    /**
     * Force stop the DQ Thread if it is running.
     *
     * Does not work when the device has been opened in blocking mode.
     *
     * @sa startDQThread, waitForDQThread
     *
     * @returns 0 for success, -1 for failure/timeout
     */
    int stopDQThread();
    /**
     * Wait for the DQ Thread to stop
     *
     * This function waits till the DQ Thread stops or timeout is reached
     *
     * @sa startDQThread, stopDQThread
     *
     * @param[in] max_wait_ms
     * @returns 0 for success, -1 for failure/timeout
     */
    int waitForDQThread(uint32_t max_wait_ms);

    pthread_mutex_t plane_lock; /**< Mutex lock used along with #plane_cond */
    pthread_cond_t plane_cond; /**< Plane condition which application can wait on
                                    to receive notifications from
                                    #qBuffer/#dqBuffer */

private:
    int &fd;     /**< FD of the V4l2 Element the plane is associated with */

    const char *plane_name; /**< Name of the plane. Could be "Output Plane" or
                                 "Capture Plane". Used only for debug logs */

    enum v4l2_buf_type buf_type; /**< Type of the stream. */

    bool blocking;  /**< Whether the V4l2 element has been opened with
                         blocking mode */

    uint32_t num_buffers;   /**< Number of buffers returned by VIDIOC_REQBUFS
                                 ioctl */
    NvBuffer **buffers;     /**< Array of NvBuffer object pointers. This array is
                                 allocated and initialized in #reqbufs. */

    uint8_t n_planes;       /**< Number of planes in the buffers */
    NvBuffer::NvBufferPlaneFormat planefmts[MAX_PLANES];
                            /**< Format of the buffer planes. This should be
                                 initialized before calling #reqbufs since this
                                 is required by the NvBuffer constructor */

    enum v4l2_memory memory_type; /**< V4l2 memory type of the buffers */

    uint32_t num_queued_buffers;  /**< Number of buffers currently queued on the
                                       plane */
    uint32_t total_queued_buffers;  /**< Total number of buffers queued on the
                                         plane */
    uint32_t total_dequeued_buffers;  /**< Total number of buffers dequeued from
                                           the plane */

    bool streamon;  /**< Boolean indicating if the plane is streaming */

    bool dqthread_running;  /**< Boolean indicating if the DQ Thread is running.
                                 Its value is toggled by the DQ Thread */
    bool stop_dqthread; /**< Boolean value used to signal the DQ Thread to stop */

    pthread_t dq_thread; /**< pthread ID of the DQ Thread */

    dqThreadCallback callback; /**< Callback function used by the DQ Thread */

    void *dqThread_data;    /**< Application supplied pointer provided as an
                                argument in #dqThreadCallback */

    /**
     * The DQ thread function.
     *
     * This function keeps on running infinitely till it is signalled to stop
     * by #stopDQThread or the #dqThreadCallback function returns false.
     * It keeps on trying to dequeue a buffer from the plane and calls the
     * #dqThreadCallback function on successful dequeue
     *
     * @param[in] v4l2_element_plane Pointer to the NvV4l2ElementPlane object
     *                 for which the thread has been started
     */
    static void *dqThread(void *v4l2_element_plane);

    NvElementProfiler &v4l2elem_profiler; /**< A reference to the profiler belonging
                                            to the plane's parent element. */

    /**
     * Indicates if the plane encountered an error during its operation.
     *
     * @return 0 if no error was encountered, a non-zero value if an
     *            error was encountered.
     */
    inline int isInError()
    {
        return is_in_error;
    }

    /**
     * Creates a new V4l2Element plane
     *
     *
     * @param[in] buf_type Type of the stream
     * @param[in] device_name Name of the element the plane belongs to
     * @param[in] fd FD of the device opened using v4l2_open
     * @param[in] blocking Whether the device has been opened with blocking mode
     */
    NvV4l2ElementPlane(enum v4l2_buf_type buf_type, const char *device_name,
                     int &fd, bool blocking, NvElementProfiler &profiler);

    /**
     * Disallow copy constructor.
     */
    NvV4l2ElementPlane(const NvV4l2ElementPlane& that);
    /**
     * Disallow assignment.
     */
    void operator=(NvV4l2ElementPlane const&);

    /**
     * NvV4l2ElementPlane destructor.
     *
     * Calls #deinitPlane internally.
     */
     ~NvV4l2ElementPlane();

    int is_in_error;        /**< Indicates if an error was encountered during
                               the operation of the element. */
    const char *comp_name;  /**< Specifies the name of the component,
                               for debugging. */

    friend class NvV4l2Element;
};

#endif
