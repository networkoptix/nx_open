#ifdef _WIN32

#pragma once

#include <dshow.h>
#include <windows.h>
#include <comip.h>
#include <atlmem.h>
#include <ATLComMem.h>

#include "device/device_data.h"

#include "camera/camera_plugin_types.h"

namespace nx {
namespace device {
namespace impl {

/**
 * initializes dshow for the thread that calls the public util functions
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms695279(v=vs.85).aspx
 * note: only use this from the public api functions listed in "dshow_utils.h"
 * todo: This is a work around, we shouldn't have to init and deinit every time the util
 *      functions are called.
 */
struct DShowInitializer{
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

        // todo figure out how to call Couninitialize() eventually if S_OK was returned
    }
};

std::string getDeviceName(const char * devicePath);

std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> getSupportedCodecs(const char * devicePath);

std::vector<DeviceData> getDeviceList();

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const std::shared_ptr<AbstractCompressionTypeDescriptor>& targetCodecID);

void setBitrate(const char * devicePath, int bitrate, nxcip::CompressionType targetCodecID);

int getMaxBitrate(const char * devicePath, nxcip::CompressionType targetCodecID);


///////////////////////////// addtiional functions that help with the api /////////////////////////

std::vector<DeviceData> getDeviceList(REFGUID category);

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
HRESULT findDevice(REFGUID category, const char * devicePath, IMoniker ** outMoniker);

HRESULT getSupportedCodecs(IMoniker *pMoniker, std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>>* codecList);
HRESULT getResolutionList(IMoniker *pMoniker,
    std::vector<ResolutionData>* outResolutionList,
    nxcip::CompressionType codecID);

HRESULT getBitrateList(IMoniker *pMoniker,
    std::vector<int>* outBitrateList,
    nxcip::CompressionType targetCodec);

std::string toStdString(BSTR str);

std::string getDeviceName(IMoniker *pMoniker);
std::string getDevicePath(IMoniker *pMoniker);
std::string getWaveInID(IMoniker * pMoniker);

} // namespace impl
} // namespace device
} // namespace nx

#endif
