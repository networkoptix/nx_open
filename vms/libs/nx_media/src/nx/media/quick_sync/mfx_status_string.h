// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <vpl/mfx.h>

QString toString(mfxStatus status)
{
    switch (status)
    {
        case MFX_ERR_NONE:
            return QString("no error");

        /* reserved for unexpected errors */
        case MFX_ERR_UNKNOWN:
            return QString("unknown error");

        /* error codes <0 */
        case MFX_ERR_NULL_PTR:
            return QString("null pointer");
        case MFX_ERR_UNSUPPORTED:
            return QString("undeveloped feature");
        case MFX_ERR_MEMORY_ALLOC:
            return QString("failed to allocate memory");
        case MFX_ERR_NOT_ENOUGH_BUFFER:
            return QString("insufficient buffer at input");
        case MFX_ERR_INVALID_HANDLE:
            return QString("invalid handle");
        case MFX_ERR_LOCK_MEMORY:
            return QString("failed to lock the memory block");
        case MFX_ERR_NOT_INITIALIZED:
            return QString("member function called before initialization");
        case MFX_ERR_NOT_FOUND:
            return QString("the specified object is not found");
        case MFX_ERR_MORE_DATA:
            return QString("expect more data at input");
        case MFX_ERR_MORE_SURFACE:
            return QString("expect more surface at output");
        case MFX_ERR_ABORTED:
            return QString("operation aborted");
        case MFX_ERR_DEVICE_LOST:
            return QString("lose the HW acceleration device");
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            return QString("incompatible video parameters");
        case MFX_ERR_INVALID_VIDEO_PARAM:
            return QString("invalid video parameters");
        case MFX_ERR_UNDEFINED_BEHAVIOR:
            return QString("undefined behavior");
        case MFX_ERR_DEVICE_FAILED:
            return QString("device operation failure");
        case MFX_ERR_MORE_BITSTREAM:
            return QString("expect more bitstream buffers at output");

        /* warnings >0 */
        case MFX_WRN_IN_EXECUTION:
            return QString("the previous asynchrous operation is in execution");
        case MFX_WRN_DEVICE_BUSY:
            return QString("the HW acceleration device is busy");
        case MFX_WRN_VIDEO_PARAM_CHANGED:
            return QString("the video parameters are changed during decoding");
        case MFX_WRN_PARTIAL_ACCELERATION:
            return QString("SW is used");
        case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
            return QString("incompatible video parameters");
        case MFX_WRN_VALUE_NOT_CHANGED:
            return QString("the value is saturated based on its valid range");
        case MFX_WRN_OUT_OF_RANGE:
            return QString("the value is out of valid range");

        /* threading statuses */
        case MFX_TASK_WORKING:
            return QString("there is some more work to do");
        case MFX_TASK_BUSY:
            return QString("task is waiting for resources");
        default:
            return QString("invalid error code");
    }
}
