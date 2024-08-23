// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rpi_omx.h"

namespace rpi_omx
{
    std::atomic<uint32_t> OMXExeption::m_lastError;
    std::atomic<uint32_t> OMXExeption::m_eventError;

    static OMX_ERRORTYPE callback_EventHandler(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_EVENTTYPE eEvent,
        OMX_U32 nData1,
        OMX_U32 nData2,
        OMX_PTR pEventData);

    static OMX_ERRORTYPE callback_EmptyBufferDone(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_BUFFERHEADERTYPE * pBuffer);

    static OMX_ERRORTYPE callback_FillBufferDone(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_BUFFERHEADERTYPE * pBuffer);

    OMX_CALLBACKTYPE cbsEvents = {
        .EventHandler = callback_EventHandler,
        .EmptyBufferDone = callback_EmptyBufferDone,
        .FillBufferDone = callback_FillBufferDone
    };

    //

    static OMX_ERRORTYPE callback_EventHandler(
            OMX_HANDLETYPE hComponent,
            OMX_PTR pAppData,
            OMX_EVENTTYPE eEvent,
            OMX_U32 nData1,
            OMX_U32 nData2,
            OMX_PTR /*pEventData*/)
    {
        Component * component = static_cast<Component *>(pAppData);

        printEvent(component->name(), hComponent, eEvent, nData1, nData2);

        // commented: synced by atomic variables
        //Lock lock(component->semaphore());  // LOCK

        switch (eEvent)
        {
            case OMX_EventCmdComplete:
            {
                switch (nData1)
                {
                    case OMX_CommandFlush:
                    case OMX_CommandPortDisable:
                    case OMX_CommandPortEnable:
                    case OMX_CommandMarkBuffer:
                        // nData2 is port index
                        component->eventCmdComplete(nData1, nData2);
                        break;

                    case OMX_CommandStateSet:
                        // nData2 is state
                        component->eventCmdComplete(nData1, nData2);
                        break;

                    default:
                        break;
                }

                break;
            }

            case OMX_EventPortSettingsChanged:
            {
                // nData1 is port index
                component->eventPortSettingsChanged(nData1);
                break;
            }

            // vendor specific
            case OMX_EventParamOrConfigChanged:
            {
                if (nData2 == OMX_IndexParamCameraDeviceNumber)
                {
                    Camera * camera = static_cast<Camera *>(pAppData);
                    camera->eventReady();
                }

                break;
            }

            case OMX_EventError:
            {
                debug_print("OMX_EventError: 0x%x 0x%x\n", nData1, nData2);

                switch ((int) nData1)
                {
                case OMX_ErrorPortUnpopulated:
                    // not an error
                    break;

                default:
                    OMXExeption::setEventError(nData1);
                    break;
                }

                break;
            }

            case OMX_EventMark:
            case OMX_EventResourcesAcquired:
            case OMX_EventBufferFlag:
            default:
                break;
        }

        return OMX_ErrorNone;
    }

    static OMX_ERRORTYPE callback_EmptyBufferDone(OMX_HANDLETYPE, OMX_PTR, OMX_BUFFERHEADERTYPE *)
    {
        return OMX_ErrorNone;
    }

    static OMX_ERRORTYPE callback_FillBufferDone(OMX_HANDLETYPE /*hComponent*/, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE * /*pBuffer*/)
    {
        Component * component = static_cast<Component *>(pAppData);

        // commented: synced by atomic variables
        //Lock lock(component->semaphore());  // LOCK

        // Only Encoders use CPU<->GPU Buffer. Others are tunneled => no events for them here.
        if (component->type() == Encoder::cType)
        {
            Encoder * encoder = static_cast<Encoder *>(pAppData);
            encoder->outBuffer().setFilled();
        }

        return OMX_ErrorNone;
    }
}
