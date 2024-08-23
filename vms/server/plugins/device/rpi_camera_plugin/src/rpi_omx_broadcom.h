// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef _RPI_BROADCOM_H_
#define _RPI_BROADCOM_H_

#include <interface/vcos/vcos_semaphore.h>
#include <interface/vmcs_host/vchost.h>

/// Broadcom specific Video Core and OpenMAX IL stuff
namespace broadcom
{
    typedef enum
    {
        VIDEO_SCHEDULER = 10,
        SOURCE = 20,
        RESIZER = 60,
        CAMERA = 70,
        CLOCK = 80,
        VIDEO_RENDER = 90,
        VIDEO_DECODER = 130,
        VIDEO_ENCODER = 200,
        EGL_RENDER = 220,
        NULL_SINK = 240,
        VIDEO_SPLITTER = 250
    } ComponentType;

    static const char * componentType2name(ComponentType type)
    {
        switch (type)
        {
            case VIDEO_SCHEDULER:   return "OMX.broadcom.video_scheduler";
            case SOURCE:            return "OMX.broadcom.source";
            case RESIZER:           return "OMX.broadcom.resize";
            case CAMERA:            return "OMX.broadcom.camera";
            case CLOCK:             return "OMX.broadcom.clock";
            case VIDEO_RENDER:      return "OMX.broadcom.video_render";
            case VIDEO_DECODER:     return "OMX.broadcom.video_decode";
            case VIDEO_ENCODER:     return "OMX.broadcom.video_encode";
            case EGL_RENDER:        return "OMX.broadcom.egl_render";
            case NULL_SINK:         return "OMX.broadcom.null_sink";
            case VIDEO_SPLITTER:    return "OMX.broadcom.video_splitter";
        }

        return nullptr;
    }

    static unsigned componentPortsCount(ComponentType type)
    {
        switch (type)
        {
            case VIDEO_SCHEDULER:   return 3;
            case SOURCE:            return 1;
            case RESIZER:           return 2;
            case CAMERA:            return 4;
            case CLOCK:             return 6;
            case VIDEO_RENDER:      return 1;
            case VIDEO_DECODER:     return 2;
            case VIDEO_ENCODER:     return 2;
            case EGL_RENDER:        return 2;
            case NULL_SINK:         return 3;
            case VIDEO_SPLITTER:    return 5;
        }

        return 0;
    }

    struct VcosSemaphore
    {
        VcosSemaphore(const char * name)
        {
            if (vcos_semaphore_create(&sem_, name, 1) != VCOS_SUCCESS)
                throw "Failed to create handler lock semaphore";
        }

        ~VcosSemaphore()
        {
            vcos_semaphore_delete(&sem_);
        }

        VCOS_STATUS_T wait() { return vcos_semaphore_wait(&sem_); }
        VCOS_STATUS_T post() { return vcos_semaphore_post(&sem_); }

    private:
        VCOS_SEMAPHORE_T sem_;
    };

    class VcosLock
    {
    public:
        VcosLock(VcosSemaphore * sem)
        :   sem_(sem)
        {
            sem_->wait();
        }

        ~VcosLock()
        {
            sem_->post();
        }

    private:
        VcosSemaphore * sem_;
    };
}

#endif
