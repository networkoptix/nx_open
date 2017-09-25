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

#ifndef _ARGUS_CAPTURE_METADATA_H
#define _ARGUS_CAPTURE_METADATA_H


namespace Argus
{

/**
 * A container for metadata from a single capture.
 */
class CaptureMetadata : public InterfaceProvider
{
protected:
    ~CaptureMetadata() {}
};

/**
 * An object that holds a CaptureMetadata instance
 * and is used to read the capture metadata from a consumer thread directly from
 * the embedded EGLStream metadata. It exposes the same interfaces as
 * CaptureMetadata, but is owned by the consumer.
 */
class CaptureMetadataContainer : public InterfaceProvider, public Destructable
{
protected:
    ~CaptureMetadataContainer() {}
};


/**
 * @class IBayerHistogram
 *
 * An interface to a set of histograms created from Bayer pixel data, one per Bayer channel.
 * Bins are evenly distributed across the possible range of values,
 * and each contains the number of pixels whose value fell within the range for that bin.
 */
DEFINE_UUID(InterfaceID, IID_BAYER_HISTOGRAM, 50bc4f1c,6424,beb7,baae,b0,90,c9,c3,25,9e);

class IBayerHistogram : public Interface
{
public:

    static const InterfaceID& id() { return IID_BAYER_HISTOGRAM; }

    /**
     * Returns the number of bins (per channel) in the histogram.
     */
    virtual uint32_t getBinCount() const = 0;

    /**
     * Returns the value contained in a single bin.
     * @returns 0 if <tt>bin >= getBinCount()</tt>;
     * if @c status is non-NULL, STATUS_INVALID_PARAMS will be returned there.
     */
    virtual uint32_t getBinData(BayerChannel channel, uint32_t bin,
                                Status* status = NULL) const = 0;

protected:

    ~IBayerHistogram() {}
};


/**
 * @class IRGBHistogram
  *
 * An interface to a set of histograms created from RGB pixel data, one per RGB channel.
 * Bins are evenly distributed across the possible range of values,
 * and each contains the number of pixels whose value fell within the range for that bin.
 */
 DEFINE_UUID(InterfaceID, IID_RGB_HISTOGRAM, 50bc4f1d,6424,beb7,baae,b0,90,c9,c3,25,9e);

class IRGBHistogram : public Interface
{
public:

    static const InterfaceID& id() { return IID_RGB_HISTOGRAM; }

    /**
     * Returns the number of bins (per channel) in the histogram.
     */
    virtual uint32_t getBinCount() const = 0;

    /**
     * Returns the value contained in a single bin.
     * @returns 0 if <tt>bin >= getBinCount()</tt>;
     * if @c status is non-NULL, STATUS_INVALID_PARAMS will be returned there.
     */
    virtual uint32_t getBinData(RGBColorChannel channel, uint32_t bin,
                                Status* status = NULL) const = 0;

protected:

    ~IRGBHistogram() {}
};


/**
 * @class IStreamCaptureMetadata
 *
 * An interface that reports metadata for a single stream in a single capture.
 */
DEFINE_UUID(InterfaceID, IID_STREAM_CAPTURE_METADATA, 61cbe0a8,0ee1,4c67,baae,02,02,1a,b8,d9,47);

class IStreamCaptureMetadata : public Interface
{
public:

    static const InterfaceID& id() { return IID_STREAM_CAPTURE_METADATA; }

    /**
     * Returns the clip rectangle used for this stream.
     */
    virtual NormalizedRect getSourceClipRect() const = 0;

protected:

    ~IStreamCaptureMetadata() {}
};


/**
 * @class ICaptureMetadata
 *
 * An interface that reports metadata for a single capture.
 */
DEFINE_UUID(InterfaceID, IID_CAPTURE_METADATA, 5f6ac5d4,59e8,45d0,8bac,38,09,1f,f8,74,a9);

class ICaptureMetadata : public Interface
{
public:

    static const InterfaceID& id() { return IID_CAPTURE_METADATA; }

    static const uint32_t NUM_COLOR_CORRECTION_ELEMENTS = 9;
    static const uint32_t NUM_AWB_WB_ESTIMATE_ELEMENTS = 4;

    /**
     * Returns the capture id (the value returned from ICaptureSession::capture())
     * of the capture that generated this metadata.
     */
    virtual uint32_t getCaptureId() const = 0;

    /**
     * Returns the @c clientData value for the Request used in the capture
     * that generated this metadata.
     *
     * @see ICaptureSession::createRequest()
     */
    virtual uint32_t getClientData() const = 0;

    /**
     * Returns the per-stream metadata provider for @c stream.
     * This object will provide the IStreamCaptureMetadata interface.
     */
    virtual InterfaceProvider* getStreamMetadata(const OutputStream* stream) const = 0;

    /**
     * Returns the Bayer histogram for this capture.
     * This object will provide the IBayerHistogram interface.
     */
    virtual const InterfaceProvider* getBayerHistogram() const = 0;

    /**
     * Returns the RGB histogram for this capture.
     * This object will provide the IRGBHistogram interface.
     */
    virtual const InterfaceProvider* getRGBHistogram() const = 0;

    /**
     * Returns true if and only if AE was locked for this capture.
     */
    virtual bool getAeLocked() const = 0;

    /**
     * Gets the AE regions of interest used for this capture.
     *
     * @param[in] regions A vector that will be populated with the AE regions used in capture.
     *
     * @returns success/status of the call.
     */
    virtual Status getAeRegions(std::vector<AcRegion>* regions) const = 0;

    /**
     * Returns the state of AE when it ran for this capture.
     */
    virtual AeState getAeState() const = 0;

    /**
     * Returns the focuser position used for this capture (in focuser steps).
     */
    virtual int32_t getFocuserPosition() const = 0;

    /**
     * Returns the CCT calculated by AWB for this capture.
     */
    virtual uint32_t getAwbCct() const = 0;

    /**
     * Returns the AWB gains used for this capture.
     */
    virtual BayerTuple<float> getAwbGains() const = 0;

    /**
     * Returns the AWB mode used for this capture.
     */
    virtual AwbMode getAwbMode() const = 0;

    /**
     * Gets the AWB regions of interest used for this capture.
     *
     * @param[in] regions A vector that will be populated with the AWB regions used in capture.
     *
     * @returns success/status of the call.
     */
    virtual Status getAwbRegions(std::vector<AcRegion>* regions) const = 0;

    /**
     * @returns the state of AWB when it ran for this capture.
     */
    virtual AwbState getAwbState() const = 0;

    /**
     * Gets the current AWB WB estimate as a vector of NUM_AWB_WB_ESTIMATE_ELEMENTS values.
     * @todo DOC describe getAwbWbEstimate() really.
     *
     * @param[in] estimate A vector that will be populated with the AWB WB estimates.
     *
     * @returns success/status of the call.
     */
    virtual Status getAwbWbEstimate(std::vector<float>* estimate) const = 0;

    /**
     * Returns the enable state for the client-supplied Color Correction Matrix.
     */
    virtual bool getColorCorrectionMatrixEnable() const = 0;

    /**
     * Gets the 3x3 client-supplied Color Correction Matrix as a flattened
     * (row-major) vector of 9 values.  This matrix is irrelevant if
     * <tt>getColorCorrectionMatrixEnable() == false</tt>.
     *
     * @param[in] ccMatrix A vector that will be populated with the color correction matrix.
     *
     * @returns success/status of the call.
     */
    virtual Status getColorCorrectionMatrix(std::vector<float>* ccMatrix) const = 0;

    /**
     * Returns the color saturation value used for this capture (including biasing).
     */
    virtual float getColorSaturation() const = 0;

    /**
     * Returns the time it took to integrate this capture (in nanoseconds).
     * @todo DOC a more precise description
     */
    virtual uint64_t getFrameDuration() const = 0;

    /**
     * Returns the digital gain used for this capture.
     */
    virtual float getIspDigitalGain() const = 0;

    /**
     * Returns the frame readout time for this capture (in nanoseconds) from
     * the beginning of the first line to the beginning of the last line.
     */
    virtual uint64_t getFrameReadoutTime() const = 0;

    /**
     * Returns the estimated scene brightness for this capture (in lux).
     */
    virtual float getSceneLux() const = 0;

    /**
     * Returns the sensor analog gain used for this capture.
     */
    virtual float getSensorAnalogGain() const = 0;

    /**
     * Returns the sensor exposure time for this capture (in nanoseconds).
     */
    virtual uint64_t getSensorExposureTime() const = 0;

    /**
     * Returns the ISO value used for this capture.
     */
    virtual uint32_t getSensorSensitivity() const = 0;

    /**
     * Returns the start timestamp for the sensor (in nanoseconds).
     * This is the time that the first data from this capture arrives from the sensor.
     */
    virtual uint64_t getSensorTimestamp() const = 0;

    /**
     * Returns true if and only if a user-specified tone map curve was used for this capture.
     */
    virtual bool getToneMapCurveEnabled() const = 0;

    /**
     * Returns the specified color channel for the tone mapping table
     * (as a vector of NUM_TONE_MAP_ELEMENTS values). These values are irrelevant if
     * <tt>getToneMapCurveEnabled() == false</tt>.
     *
     * @param[in] channel Specified the color channel for which the curve will be returned.
     * @param[in] curve A vector that will be populated with the color curve used in capture.
     *
     * @returns success/status of the call.
     */
    virtual Status getToneMapCurve(RGBColorChannel channel, std::vector<float>* curve) const = 0;

protected:
    ~ICaptureMetadata() {}
};

/**
 * @class IDenoiseMetadata
 * - Exposed by CaptureMetadata objects.
 */
DEFINE_UUID(InterfaceID, IID_DENOISE_METADATA, 7A461D23,6AE1,11E6,BDF4,08,00,20,0C,9A,66);

class IDenoiseMetadata : public Interface
{
public:
    static const InterfaceID& id() { return IID_DENOISE_METADATA; }

    /**
     * Returns the denoise mode used for the capture.
     */
    virtual DenoiseMode getDenoiseMode() const = 0;

    /**
     * Returns the denoise strength used for the capture.
     */
    virtual float getDenoiseStrength() const = 0;

protected:
    ~IDenoiseMetadata() {}
};

/**
 * @class IEdgeEnhanceMetadata
 * - Exposed by CaptureMetadata objects.
 */
DEFINE_UUID(InterfaceID, IID_EDGE_ENHANCE_METADATA, 7A461D24,6AE1,11E6,BDF4,08,00,20,0C,9A,66);

class IEdgeEnhanceMetadata : public Interface
{
public:
    static const InterfaceID& id() { return IID_EDGE_ENHANCE_METADATA; }

    /**
     * Returns the edge enhancement mode used for the capture.
     */
    virtual EdgeEnhanceMode getEdgeEnhanceMode() const = 0;

    /**
     * Returns the edge enhancement strength used for the capture.
     */
    virtual float getEdgeEnhanceStrength() const = 0;

protected:
    ~IEdgeEnhanceMetadata() {}
};

/**
 * @class IVideoStabilizationMetadata
 * - Exposed by CaptureMetadata objects.
 */
DEFINE_UUID(InterfaceID, IID_VIDEO_STABILIZATION_METADATA, 7A461D25,6AE1,11E6,BDF4,08,00,20,0C,9A,66);

class IVideoStabilizationMetadata : public Interface
{
public:
    static const InterfaceID& id() { return IID_VIDEO_STABILIZATION_METADATA; }

    /**
     * Returns the video stabilization mode used for the capture.
     */
    virtual VideoStabilizationMode getVideoStabilizationMode() const = 0;

protected:
    ~IVideoStabilizationMetadata() {}
};

} // namespace Argus

#endif // _ARGUS_CAPTURE_METADATA_H
