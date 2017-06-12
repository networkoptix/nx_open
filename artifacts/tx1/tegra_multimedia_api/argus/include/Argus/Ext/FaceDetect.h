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

#ifndef _ARGUS_FACE_DETECT_H
#define _ARGUS_FACE_DETECT_H

namespace Argus
{

/**
 * The Ext::FaceDetect extension adds internal face-detection algorithms to
 * the Argus driver. It introduces four new interfaces:
 *   - IFaceDetectCaps; exposes the face detection capabilities of a CaptureSession.
 *   - IFaceDetectSettings; used to enable face detection for a Request.
 *   - IFaceDetectMetadata; returns a list of FaceDetectResult objects from a
 *                          completed capture's CaptureMetadata.
 *   - IFaceDetectResult; exposes the image rect and confidence level of a result object
 *                        returned by getFaceDetectResults.
 */
DEFINE_UUID(ExtensionName, EXT_FACE_DETECT, 40412bb0,ba24,11e5,a837,08,00,20,0c,9a,66);

namespace Ext
{

/**
 * @class IFaceDetectCaps
 *
 * Exposes the face detect capabilities of a CaptureSession.
 */
DEFINE_UUID(InterfaceID, IID_FACE_DETECT_CAPS, 40412bb0,ba24,11e5,a837,08,00,20,0c,9a,66);

class IFaceDetectCaps : public Interface
{
public:
    static const InterfaceID& id() { return IID_FACE_DETECT_CAPS; }

    /**
     * Returns the maximum number of faces that can be detected by the face detection
     * algorithm per request. Returned value must be >= 1.
     */
    virtual uint32_t getMaxFaceDetectResults() const = 0;

protected:
    ~IFaceDetectCaps() {}
};

/**
 * @class IFaceDetectSettings
 *
 * Request settings used to configure face detection.
 */
DEFINE_UUID(InterfaceID, IID_FACE_DETECT_SETTINGS, 40412bb1,ba24,11e5,a837,08,00,20,0c,9a,66);

class IFaceDetectSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_FACE_DETECT_SETTINGS; }

    /**
     * Enables or disables face detection. When face detection is enabled the CaptureMetadata
     * returned by completed captures will expose the IFaceDetectMetadata interface and the
     * FaceDetectResults returned by this interface will expose the IFaceDetectResults interface.
     * @param[in] enable whether or not face detection is enabled.
     */
    virtual void setFaceDetectEnable(bool enable) = 0;

    /**
     * @returns whether or not face detection is enabled.
     */
    virtual bool getFaceDetectEnable() const = 0;

protected:
    ~IFaceDetectSettings() {}
};

/**
 * @class IFaceDetectMetadata
 *
 * Exposes the face detect result objects in a CaptureMetadata object.
 * These results objects must support at least IFaceDetectResult.
 */
DEFINE_UUID(InterfaceID, IID_FACE_DETECT_METADATA, 40412bb2,ba24,11e5,a837,08,00,20,0c,9a,66);

class IFaceDetectMetadata : public Interface
{
public:
    static const InterfaceID& id() { return IID_FACE_DETECT_METADATA; }

    /**
     * @returns the face detection results.
     * @param[out] results, a vector that will be populated with the face detect results.
     *
     * @returns success/status of the call.
     */
    virtual Status getFaceDetectResults(std::vector<InterfaceProvider*>* results) const = 0;

protected:
    ~IFaceDetectMetadata() {}
};

/**
 * @class IFaceDetectResult
 *
 * Exposes the image rect and confidence level of face detect result object returned
 * by IFaceDetectMetadata.
 */
DEFINE_UUID(InterfaceID, IID_FACE_DETECT_RESULT, 40412bb3,ba24,11e5,a837,08,00,20,0c,9a,66);

class IFaceDetectResult : public Interface
{
public:
    static const InterfaceID& id() { return IID_FACE_DETECT_RESULT; }

    /**
     * @returns the normlized coordinates of the region containing the face, relative
     *  to the uncropped image sensor mode size.
     */
    virtual NormalizedRect getRect() const = 0;

    /**
     * @returns the confidence level of the result. This confidence is in the range
     *  [0, 1], where 1 is the highest confidence. For a typical application that
     *  highlights faces in a scene, filtering results to ignore those with a
     *  confidence less than 0.5 is suggested.
     */
    virtual float getConfidence() const = 0;

protected:
    ~IFaceDetectResult() {}
};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_FACE_DETECT_H
