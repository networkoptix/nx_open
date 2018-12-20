#ifdef _WIN32

#pragma once

#include <vector>
#include <string>

#include <dshow.h>
#include <windows.h>
#include <comip.h>
#include <atlmem.h>
#include <ATLComMem.h>

#include <camera/camera_plugin_types.h>

#include "device/device_data.h"
#include "device/abstract_compression_type_descriptor.h"
#include "device/video/resolution_data.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace detail {

/**
 * initializes dshow for the thread that calls the public util functions
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms695279(v=vs.85).aspx
 * Note: only use this from the public api functions listed in "dshow_utils.h"
 * TODO: This is a work around, we shouldn't have to init and deinit every time the util
 *      functions are called.
 */
struct DShowInitializer
{
    HRESULT m_result;

    DShowInitializer():
        m_result(CoInitialize(NULL))
    {
    }

    ~DShowInitializer()
    {
        // S_OK should not uninit immediately:
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms678543(v=vs.85).aspx
        if (S_FALSE == m_result)
            CoUninitialize();

        // TODO: figure out how to call Couninitialize() eventually if S_OK was returned
    }
};

//-------------------------------------------------------------------------------------------------
// public api

std::string getDeviceName(const std::string& devicePath);

std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const std::string& devicePath);

std::vector<DeviceData> getDeviceList();

std::vector<ResolutionData> getResolutionList(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

void setBitrate(
    const std::string& devicePath,
    int bitrate,
    const CompressionTypeDescriptorPtr& targetCodecID);

int getMaxBitrate(const std::string& devicePath, const CompressionTypeDescriptorPtr& targetCodecID);

//-------------------------------------------------------------------------------------------------
// api helper functions

void freeMediaType(AM_MEDIA_TYPE& mediaType);
void deleteMediaType(AM_MEDIA_TYPE *mediaType);
bool isVideoInfo(AM_MEDIA_TYPE* mediaType);
nxcip::CompressionType toNxCodecID(DWORD biCompression);

HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION pinDirection, IPin **ppPin);
HRESULT getDeviceProperty(IMoniker * pMoniker, LPCOLESTR propName, VARIANT * outVar);
HRESULT enumerateDevices(REFGUID category, IEnumMoniker **ppEnum);
HRESULT enumerateMediaTypes(
    IMoniker* pMoniker,
    IEnumMediaTypes ** outEnumMediaTypes,
    IPin** outPin = NULL);

HRESULT findDevice(REFGUID category, const std::string& devicePath, IMoniker ** outMoniker);

HRESULT getSupportedCodecs(
    IMoniker *pMoniker,
    std::vector<device::CompressionTypeDescriptorPtr>* codecList);

HRESULT getResolutionList(IMoniker *pMoniker,
    std::vector<ResolutionData>* outResolutionList,
    const device::CompressionTypeDescriptorPtr& targetCodec);

HRESULT getBitrateList(IMoniker *pMoniker,
    std::vector<int>* outBitrateList,
    const device::CompressionTypeDescriptorPtr& targetCodec);

std::string toStdString(BSTR str);
std::string getDeviceName(IMoniker *pMoniker);
std::string getDevicePath(IMoniker *pMoniker);

//-------------------------------------------------------------------------------------------------
// audio

struct AudioDeviceDescriptor
{
    DeviceData data;
    LONG waveInID;

    AudioDeviceDescriptor(const DeviceData & data, LONG waveInID):
        data(data),
        waveInID(waveInID)
    {
    }

    bool isDefault() const
    {
        return waveInID == 0;
    }
};

LONG getWaveInID(IMoniker * pMoniker);
std::string getDisplayName(IMoniker * pMoniker);
std::vector<AudioDeviceDescriptor> getAudioDeviceList();

} // namespace detail
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif
