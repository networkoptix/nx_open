#ifndef __eglext_nv_h_
#define __eglext_nv_h_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (c) 2008 - 2015, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef EGL_EXT_stream_acquire_mode
#define EGL_EXT_stream_acquire_mode 1
#define EGL_CONSUMER_AUTO_ACQUIRE_EXT         0x332B
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREATTRIBEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerAcquireAttribEXT (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif
#endif /* EGL_EXT_stream_acquire_mode */

#ifndef EGL_EXT_stream_consumer_qnxscreen_window
#define EGL_EXT_stream_consumer_qnxscreen_window 1
#define EGL_CONSUMER_ACQUIRE_QNX_FLUSHING_EXT        0x3320
#define EGL_CONSUMER_ACQUIRE_QNX_DISPNO_EXT          0x3321
#define EGL_CONSUMER_ACQUIRE_QNX_LAYERNO_EXT         0x3322
#define EGL_CONSUMER_ACQUIRE_QNX_SURFACE_TYPE_EXT    0x3323
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_POS_X_EXT   0x3324
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_POS_Y_EXT   0x3325
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_WIDTH_EXT   0x3326
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_HEIGHT_EXT  0x3327
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERQNXSCREENWINDOWEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerQNXScreenWindowEXT (EGLDisplay dpy, EGLStreamKHR stream);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerAcquireEXT (EGLDisplay dpy, EGLStreamKHR stream);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerReleaseEXT (EGLDisplay dpy, EGLStreamKHR stream);
#endif
#endif /* EGL_EXT_stream_consumer_qnxscreen_window */

#ifndef EGL_NV_output_drm_atomic
#define EGL_NV_output_drm_atomic 1
#define EGL_DRM_ATOMIC_REQUEST_NV             0x3333
#endif /* EGL_NV_output_drm_atomic */

#ifndef EGL_NV_perfmon
#define EGL_NV_perfmon 1
#define EGL_PERFMONITOR_HARDWARE_COUNTERS_BIT_NV    0x00000001
#define EGL_PERFMONITOR_OPENGL_ES_API_BIT_NV        0x00000010
#define EGL_PERFMONITOR_OPENVG_API_BIT_NV           0x00000020
#define EGL_PERFMONITOR_OPENGL_ES2_API_BIT_NV       0x00000040
#define EGL_COUNTER_NAME_NV                         0x3220
#define EGL_COUNTER_DESCRIPTION_NV                  0x3221
#define EGL_IS_HARDWARE_COUNTER_NV                  0x3222
#define EGL_COUNTER_MAX_NV                          0x3223
#define EGL_COUNTER_VALUE_TYPE_NV                   0x3224
#define EGL_RAW_VALUE_NV                            0x3225
#define EGL_PERCENTAGE_VALUE_NV                     0x3226
#define EGL_BAD_CURRENT_PERFMONITOR_NV              0x3227
#define EGL_NO_PERFMONITOR_NV ((EGLPerfMonitorNV)0)
#define EGL_DEFAULT_PERFMARKER_NV ((EGLPerfMarkerNV)0)
typedef void *EGLPerfMonitorNV;
typedef void *EGLPerfCounterNV;
typedef void *EGLPerfMarkerNV;
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLPerfMonitorNV EGLAPIENTRY eglCreatePerfMonitorNV(EGLDisplay dpy, EGLint mask);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyPerfMonitorNV(EGLDisplay dpy, EGLPerfMonitorNV monitor);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrentPerfMonitorNV(EGLPerfMonitorNV monitor);
EGLAPI EGLPerfMonitorNV EGLAPIENTRY eglGetCurrentPerfMonitorNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfCountersNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV *counters, EGLint counter_size, EGLint *num_counter);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfCounterAttribNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname, EGLint *value);
EGLAPI const char * EGLAPIENTRY eglQueryPerfCounterStringNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorAddCountersNV(EGLint n, const EGLPerfCounterNV *counters);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorRemoveCountersNV(EGLint n, const EGLPerfCounterNV *counters);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorRemoveAllCountersNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorBeginExperimentNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorEndExperimentNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorBeginPassNV(EGLint n);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorEndPassNV(void);
EGLAPI EGLPerfMarkerNV EGLAPIENTRY eglCreatePerfMarkerNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyPerfMarkerNV(EGLPerfMarkerNV marker);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrentPerfMarkerNV(EGLPerfMarkerNV marker);
EGLAPI EGLPerfMarkerNV EGLAPIENTRY eglGetCurrentPerfMarkerNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfMarkerCounterNV(EGLPerfMarkerNV marker, EGLPerfCounterNV counter, EGLuint64NV *value, EGLuint64NV *cycles);
EGLAPI EGLBoolean EGLAPIENTRY eglValidatePerfMonitorNV(EGLint *num_passes);
#endif
typedef EGLPerfMonitorNV (EGLAPIENTRYP PFNEGLCREATEPERFMONITORNVPROC)(EGLDisplay dpy, EGLint mask);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYPERFMONITORNVPROC)(EGLDisplay dpy, EGLPerfMonitorNV monitor);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLMAKECURRENTPERFMONITORNVPROC)(EGLPerfMonitorNV monitor);
typedef EGLPerfMonitorNV (EGLAPIENTRYP PFNEGLGETCURRENTPERFMONITORNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFCOUNTERSNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV *counters, EGLint counter_size, EGLint *num_counter);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFCOUNTERATTRIBNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname, EGLint *value);
typedef const char * (EGLAPIENTRYP PFNEGLQUERYPERFCOUNTERSTRINGNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORADDCOUNTERSNVPROC)(EGLint n, const EGLPerfCounterNV *counters);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORREMOVECOUNTERSNVPROC)(EGLint n, const EGLPerfCounterNV *counters);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORREMOVEALLCOUNTERSNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORBEGINEXPERIMENTNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORENDEXPERIMENTNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORBEGINPASSNVPROC)(EGLint n);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORENDPASSNVPROC)(void);
typedef EGLPerfMarkerNV (EGLAPIENTRYP PFNEGLCREATEPERFMARKERNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYPERFMARKERNVPROC)(EGLPerfMarkerNV marker);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLMAKECURRENTPERFMARKERNVPROC)(EGLPerfMarkerNV marker);
typedef EGLPerfMarkerNV (EGLAPIENTRYP PFNEGLGETCURRENTPERFMARKERNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFMARKERCOUNTERNVPROC)(EGLPerfMarkerNV marker, EGLPerfCounterNV counter, EGLuint64NV *value, EGLuint64NV *cycles);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLVALIDATEPERFMONITORNVPROC)(EGLint *num_passes);
#endif /* EGL_NV_perfmon */

#ifndef EGL_NV_quadruple_buffer
#define EGL_NV_quadruple_buffer 1
#define EGL_QUADRUPLE_BUFFER_NV             0x3231
#endif /* EGL_NV_quadruple_buffer */

#ifndef EGL_NV_secure_context
#define EGL_NV_secure_context 1
#define EGL_SECURE_NV 0x313E
#endif /* EGL_NV_secure_context */

#ifndef EGL_NV_set_renderer
#define EGL_NV_set_renderer 1
#define EGL_RENDERER_LOWEST_POWER_NV         0x313A
#define EGL_RENDERER_HIGHEST_PERFORMANCE_NV  0x313B
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglSetRendererNV(EGLenum renderer);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETRENDERERNVPROC)(EGLenum renderer);
#endif /* EGL_NV_set_renderer */

#ifndef EGL_NV_stream_consumer_gltexture_yuv
#define EGL_NV_stream_consumer_gltexture_yuv 1
#define EGL_YUV_PLANE0_TEXTURE_UNIT_NV 0x332C
#define EGL_YUV_PLANE1_TEXTURE_UNIT_NV 0x332D
#define EGL_YUV_PLANE2_TEXTURE_UNIT_NV 0x332E
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerGLTextureExternalAttribsNV(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif /* EGL_NV_stream_consumer_gltexture_yuv */

#ifndef EGL_NV_stream_cross_partition
#define EGL_NV_stream_cross_partition 1
#define EGL_STREAM_CROSS_PARTITION_NV        0x323F
#endif /* EGL_NV_stream_cross_partition */

#ifndef EGL_NV_stream_fifo_next
#define EGL_NV_stream_fifo_next 1
#define EGL_PENDING_FRAME_NV                 0x3329
#define EGL_STREAM_TIME_PENDING_NV           0x332A
#endif /* EGL_NV_stream_fifo_next */

#ifndef EGL_NV_stream_metadata
#define EGL_NV_stream_metadata 1
#define EGL_MAX_STREAM_METADATA_BLOCKS_NV            0x3250
#define EGL_MAX_STREAM_METADATA_BLOCK_SIZE_NV        0x3251
#define EGL_MAX_STREAM_METADATA_TOTAL_SIZE_NV        0x3252
#define EGL_PRODUCER_METADATA_NV                     0x3253
#define EGL_CONSUMER_METADATA_NV                     0x3254
#define EGL_PENDING_METADATA_NV                      0x3328
#define EGL_METADATA0_SIZE_NV                        0x3255
#define EGL_METADATA1_SIZE_NV                        0x3256
#define EGL_METADATA2_SIZE_NV                        0x3257
#define EGL_METADATA3_SIZE_NV                        0x3258
#define EGL_METADATA0_TYPE_NV                        0x3259
#define EGL_METADATA1_TYPE_NV                        0x325A
#define EGL_METADATA2_TYPE_NV                        0x325B
#define EGL_METADATA3_TYPE_NV                        0x325C
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglQueryDisplayAttribNV(EGLDisplay dpy, EGLint attribute, EGLAttrib* value);
EGLAPI EGLBoolean EGLAPIENTRY eglSetStreamMetadataNV(EGLDisplay dpy, EGLStreamKHR stream, EGLint n, EGLint offset, EGLint size, const void* data);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamMetadataNV(EGLDisplay dpy, EGLStreamKHR stream, EGLenum name, EGLint n, EGLint offset, EGLint size, void* data);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDISPLAYATTRIBNVPROC)(EGLDisplay dpy, EGLint attribute, EGLAttrib* value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETSTREAMMETADATANVPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLint n, EGLint offset, EGLint size, const void* data);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMMETADATANVPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum name, EGLint n, EGLint offset, EGLint size, void* data);
#endif /* EGL_NV_stream_metadata */

#ifndef EGL_NV_stream_remote
#define EGL_NV_stream_remote 1
#define EGL_STREAM_STATE_INITIALIZING_NV     0x3240
#define EGL_STREAM_TYPE_NV                   0x3241
#define EGL_STREAM_PROTOCOL_NV               0x3242
#define EGL_STREAM_ENDPOINT_NV               0x3243
#define EGL_STREAM_LOCAL_NV                  0x3244
#define EGL_STREAM_CROSS_PROCESS_NV          0x3245
#define EGL_STREAM_PROTOCOL_FD_NV            0x3246
#define EGL_STREAM_CONSUMER_NV               0x3247
#define EGL_STREAM_PRODUCER_NV               0x3248
#define EGL_SYNC_CONNECTION_NV               0x3249
#define EGL_STREAM_NV                        0x324A
#endif /* EGL_NV_stream_remote */

#ifndef EGL_NV_stream_reset
#define EGL_NV_stream_reset 1
#define EGL_SUPPORT_RESET_NV                 0x3334
#define EGL_SUPPORT_REUSE_NV                 0x3335
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglResetStreamNV(EGLDisplay dpy, EGLStreamKHR stream);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLRESETSTREAMNVPROC)(EGLDisplay dpy, EGLStreamKHR stream);
#endif /* EGL_NV_stream_reset */

#ifndef EGL_NV_stream_socket
#define EGL_NV_stream_socket 1
#define EGL_STREAM_PROTOCOL_SOCKET_NV        0x324B
#define EGL_SOCKET_HANDLE_NV                 0x324C
#define EGL_SOCKET_TYPE_NV                   0x324D
#define EGL_SOCKET_TYPE_UNIX_NV              0x324E
#define EGL_SOCKET_TYPE_INET_NV              0x324F
#endif /* EGL_NV_stream_socket */

#ifndef EGL_NV_swap_asynchronous
#define EGL_NV_swap_asynchronous
#define EGL_ASYNCHRONOUS_SWAPS_NV 0x3232
#endif /* EGL_NV_swap_asynchrounous */

#ifndef EGL_NV_swap_hint
#define EGL_NV_swap_hint
#define EGL_SWAP_HINT_NV                0x30E4
#define EGL_FASTEST_NV                  0x30E5
#endif /* EGL_NV_swap_hint */

#ifndef EGL_NV_texture_rectangle
#define EGL_NV_texture_rectangle 1
#define EGL_GL_TEXTURE_RECTANGLE_NV_KHR           0x30BB
#define EGL_TEXTURE_RECTANGLE_NV       0x20A2
#endif /* EGL_NV_texture_rectangle */

#ifndef EGL_NV_triple_buffer
#define EGL_NV_triple_buffer 1
#define EGL_TRIPLE_BUFFER_NV                0x3230
#endif /* EGL_NV_triple_buffer */

#ifndef EGL_WL_bind_wayland_display
#define EGL_WL_bind_wayland_display 1
#define EGL_WAYLAND_BUFFER_WL 0x31D5
#define EGL_WAYLAND_PLANE_WL 0x31D6
#define EGL_TEXTURE_Y_U_V_WL 0x31D7
#define EGL_TEXTURE_Y_UV_WL 0x31D8
#define EGL_TEXTURE_Y_XUXV_WL 0x31D9
#define EGL_TEXTURE_EXTERNAL_WL 0x31DA
#define EGL_TEXTURE_FORMAT 0x3080
#define EGL_WAYLAND_Y_INVERTED_WL 0x31DB
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglBindWaylandDisplayWL(EGLDisplay dpy, void *display);
EGLAPI EGLBoolean EGLAPIENTRY eglUnbindWaylandDisplayWL(EGLDisplay dpy, void *display);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryWaylandBufferWL(EGLDisplay dpy, void *buffer, EGLint attribute, int *value);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, void *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, void *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, void *buffer, EGLint attribute, void *value);
#endif /* EGL_WL_bind_wayland_display */

#ifdef __cplusplus
}
#endif

#endif
