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

#ifndef _ARGUS_CAMERA_DEVICE_H
#define _ARGUS_CAMERA_DEVICE_H


namespace Argus
{

/**
 * An object representing a single camera device.
 *
 * CameraDevices are provided by a CameraProvider and are used to
 * access the camera devices available within the system.
 * Each device is based on a single sensor or a set of synchronized sensors.
 *
 * @see ICameraProvider::getCameraDevices()
 */
class CameraDevice : public InterfaceProvider
{
protected:
    ~CameraDevice() {}
};

/**
 * @class ICameraProperties
 *
 * An interface to retrieve the properties of a CameraDevice.
 */
DEFINE_UUID(InterfaceID, IID_CAMERA_PROPERTIES, 436d2a73,c85b,4a29,bce5,15,60,6e,35,86,91);

class ICameraProperties : public Interface
{
public:
    static const InterfaceID& id() { return IID_CAMERA_PROPERTIES; }

    /**
     * Returns the camera UUID.
     * @todo DOC describe the camera UUID
     */
    virtual UUID getUUID() const = 0;

    /**
     * Returns the maximum number of regions of interest supported by AE.
     * A value of 0 means that the entire image is the only supported region of interest.
     *
     * @see IAutoControlSettings::setAeRegions()
     */
    virtual uint32_t getMaxAeRegions() const = 0;

    /**
     * Returns the maximum number of regions of interest supported by AWB.
     * A value of 0 means that the entire image is the only supported region of interest.
     *
     * @see IAutoControlSettings::setAwbRegions()
     */
    virtual uint32_t getMaxAwbRegions() const = 0;

    /**
     * Returns the available sensor modes
     * @param[out] modes, a vector that will be populated with the sensor modes.
     *
     * @returns success/status of the call.
     */
    virtual Status getSensorModes(std::vector<SensorMode*>* modes) const = 0;

    /**
     * Returns the valid range of focuser positions.
     * The units are focuser steps.
     */
    virtual Range<int32_t> getFocusPositionRange() const = 0;

    /**
     * Returns the supported aperture range.
     */
    virtual Range<float> getLensApertureRange() const = 0;

protected:
    ~ICameraProperties() {}
};

/**
 * An object representing the sensor mode of a CameraDevice.
 *
 * @see ICameraProperties::getSensorModes().
 */
class SensorMode : public InterfaceProvider
{
protected:
    ~SensorMode() {}
};

/**
 * @class ISensorMode
 *
 * An interface to retrieve the properties of a SensorMode.
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_MODE, e69015e0,db2a,11e5,a837,18,00,20,0c,9a,66);

class ISensorMode : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_MODE; }

    /**
     * Returns the image resolution, in pixels.
     */
    virtual Size getResolution() const = 0;

    /**
     * Returns the supported exposure time range (in nanoseconds).
     */
    virtual Range<uint64_t> getExposureTimeRange() const = 0;

    /**
     * Returns the supported frame duration range (in nanoseconds).
     */
    virtual Range<uint64_t> getFrameDurationRange() const = 0;

    /**
     * Returns the supported analog gain range.
     */
    virtual Range<float> getAnalogGainRange() const = 0;

    /**
     * Returns the bit depth of the image captured by the image sensor in the
     * current mode.  For example, a wide dynamic range image sensor capturing
     * 16 bits per pixel would have an input bit depth of 16.
     */
    virtual uint32_t getInputBitDepth() const = 0;

    /**
     * Returns the bit depth of the image returned from the image sensor in the
     * current mode. For example, a wide dynamic range image sensor capturing
     * 16 bits per pixel might be connected through a Camera Serial Interface
     * (CSI-3) which is limited to 12 bits per pixel. The sensor would have to
     * compress the image internally and would have an output bit depth not
     * exceeding 12.
     */
    virtual uint32_t getOutputBitDepth() const = 0;

    /**
     * Describes the type of the sensor (Bayer, Yuv, etc.) and key modes of
     * operation which are enabled in the sensor mode (Wide-dynamic range,
     * Piecewise Linear Compressed output, etc.)  (Not all sensor mode types are
     * supported in the current release
     */
    virtual SensorModeType getSensorModeType() const = 0;



protected:
    ~ISensorMode() {}
};

} // namespace Argus

#endif // _ARGUS_CAMERA_DEVICE_H
