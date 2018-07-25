#ifdef _WIN32

#include "dshow_utils.h"

#include <dshow.h>
#include <windows.h>
#include <comip.h>
#include <atlmem.h>
#include <ATLComMem.h>

namespace nx {
namespace device {
namespace impl {

//-------------------------------------------------------------------------------------------------
// Private helper functions

namespace {

void FreeMediaType(AM_MEDIA_TYPE& mt);
void DeleteMediaType(AM_MEDIA_TYPE *pmt);

HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION pinDirection, IPin **ppPin);
HRESULT getDeviceProperty(IMoniker * pMoniker, LPCOLESTR propName, VARIANT * outVar);
HRESULT enumerateDevices(REFGUID category, IEnumMoniker **ppEnum);
HRESULT findDevice(REFGUID category, const char * devicePath, IMoniker ** outMoniker);

HRESULT getSupportedCodecs(IMoniker *pMoniker, std::vector<nxcip::CompressionType>* codecList);
HRESULT getResolutionList(IMoniker *pMoniker,
    std::vector<ResolutionData>* outResolutionList,
    nxcip::CompressionType codecID);

HRESULT getBitrateList(IMoniker *pMoniker,
    std::vector<int>* outBitrateList,
    nxcip::CompressionType targetCodecID);

std::string toStdString(BSTR str);
nxcip::CompressionType toNxCodecID(DWORD biCompression);

std::string getDeviceName(IMoniker *pMoniker);
std::string getDevicePath(IMoniker *pMoniker);

/**
 * initializes dshow for the thread that calls the public util functions
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms695279(v=vs.85).aspx
 * note: only use this from the public api functions listed in "dshow_utils.h"
 * todo: This is a work around, we shouldn't have to init and deinit every time the util
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

        // todo figure out how to call Couninitialize() eventually if S_OK was returned
    }
};

std::string toStdString(BSTR bstr)
{
    char * ch = _com_util::ConvertBSTRToString(bstr);
    std::string str(ch);
    delete ch;
    return str;
}

nxcip::CompressionType toNxCodecID(DWORD biCompression)
{
    switch (biCompression)
    {
        case MAKEFOURCC('H', '2', '6', '3'):
            return nxcip::AV_CODEC_ID_H263;
        case MAKEFOURCC('M', 'J', 'P', 'G'):
            return nxcip::AV_CODEC_ID_MJPEG;
        case MAKEFOURCC('H', '2', '6', '4'):
            return nxcip::AV_CODEC_ID_H264;
            // these are all the formats supported by the plugin at the moment.
            // todo add the rest
        default:
            return nxcip::AV_CODEC_ID_NONE;
    }
}

// borrowed from https://msdn.microsoft.com/en-us/library/windows/desktop/dd375432(v=vs.85).aspx
// Release the format block for a media type.
void FreeMediaType(AM_MEDIA_TYPE& mt)
{
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID) mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // Unecessary because pUnk should not be used, but safest.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

// borrowed from https://msdn.microsoft.com/en-us/library/windows/desktop/dd375432(v=vs.85).aspx
// Delete a media type structure that was allocated on the heap.
void DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
    if (pmt != NULL)
    {
        FreeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}

HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION pinDirection, IPin **ppPin)
{
    IEnumPins* pEnum = NULL;
    IPin* pPin = NULL;
    HRESULT resultCode;

    if (ppPin == NULL)
        return E_POINTER;

    resultCode = pFilter->EnumPins(&pEnum);
    if (FAILED(resultCode))
        return resultCode;

    while (pEnum->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION pinDirectionThis;
        resultCode = pPin->QueryDirection(&pinDirectionThis);
        if (FAILED(resultCode))
        {
            pPin->Release();
            pEnum->Release();
            return resultCode;
        }
        if (pinDirection == pinDirectionThis)
        {
            // Found a match. Return the IPin pointer to the caller.
            *ppPin = pPin;
            pEnum->Release();
            return S_OK;
        }
        // Release the pin for the next time through the loop.
        pPin->Release();
    }
    // No more pins. We did not find a match.
    pEnum->Release();
    return E_FAIL;
}

HRESULT getDeviceProperty(IMoniker * pMoniker, LPCOLESTR propName, VARIANT * outVar)
{
    IPropertyBag *pPropBag = NULL;
    HRESULT resultCode = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &pPropBag);
    if (FAILED(resultCode))
        return resultCode;

    VariantInit(outVar);
    resultCode = pPropBag->Read(propName, outVar, 0);
    pPropBag->Release();
    return resultCode;
}

HRESULT enumerateMediaTypes(IMoniker* pMoniker, IEnumMediaTypes ** outEnumMediaTypes)
{
    *outEnumMediaTypes = NULL;

    IBaseFilter * baseFilter = NULL;
    HRESULT resultCode = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **) &baseFilter);
    if (FAILED(resultCode))
        return resultCode;

    IPin * pin = NULL;
    //todo find out if there are multiple output pins for a device. For now we assume only one
    resultCode = getPin(baseFilter, PINDIR_OUTPUT, &pin);
    if (SUCCEEDED(resultCode))
    {
        IEnumMediaTypes * enumMediaTypes = NULL;
        resultCode = pin->EnumMediaTypes(&enumMediaTypes);
        if (SUCCEEDED(resultCode))
            *outEnumMediaTypes = enumMediaTypes;
        pin->Release();
    }
    baseFilter->Release();
    return resultCode;
}

HRESULT enumerateDevices(REFGUID category, IEnumMoniker ** ppEnum)
{
    // Create the System Device Enumerator.
    ICreateDevEnum *pDevEnum;
    HRESULT resultCode = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(resultCode))
    {
        // Create an enumerator for the category.
        resultCode = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (resultCode == S_FALSE)
            resultCode = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.

        pDevEnum->Release();
    }
    return resultCode;
}

HRESULT findDevice(REFGUID category, const char * devicePath, IMoniker ** outMoniker)
{
    IEnumMoniker * pEnum;
    HRESULT resultCode = enumerateDevices(category, &pEnum);
    if (FAILED(resultCode))
        return resultCode;

    CComBSTR devicePathBstr(devicePath);

    IMoniker *pMoniker = NULL;
    while (S_OK == (resultCode = pEnum->Next(1, &pMoniker, NULL)))
    {
        VARIANT var;
        getDeviceProperty(pMoniker, L"DevicePath", &var);

        // don't release the moniker in this case, it's the one we are looking for
        if (wcscmp(var.bstrVal, devicePathBstr) == 0)
        {
            pEnum->Release();
            *outMoniker = pMoniker;
            return resultCode;
        }
        pMoniker->Release();
    }

    pEnum->Release();
    *outMoniker = NULL;
    return resultCode;
}

HRESULT getSupportedCodecs(IMoniker *pMoniker, std::vector<nxcip::CompressionType> *outCodecList)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT resultCode = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(resultCode))
        return resultCode;

    std::vector<nxcip::CompressionType> codecList;

    AM_MEDIA_TYPE* mediaType = NULL;
    while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
    {
        if ((mediaType->formattype == FORMAT_VideoInfo) &&
            (mediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
            (mediaType->pbFormat != NULL))
        {
            auto viHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
            BITMAPINFOHEADER *bmiHeader = &viHeader->bmiHeader;

            nxcip::CompressionType codecID = toNxCodecID(bmiHeader->biCompression);

            if (std::find(codecList.begin(), codecList.end(), codecID) == codecList.end())
                codecList.push_back(codecID);
        }
        DeleteMediaType(mediaType);
    }

    *outCodecList = codecList;
    enumMediaTypes->Release();
    return resultCode;
}

HRESULT getResolutionList(IMoniker *pMoniker,
    std::vector<ResolutionData>* outResolutionList,
    nxcip::CompressionType targetCodecID)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT resultCode = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(resultCode))
        return resultCode;

    std::vector<ResolutionData> resolutionList;

    AM_MEDIA_TYPE* mediaType = NULL;
    while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
    {
        if ((mediaType->formattype == FORMAT_VideoInfo) &&
            (mediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
            (mediaType->pbFormat != NULL))
        {
            auto viHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
            BITMAPINFOHEADER *bmiHeader = &viHeader->bmiHeader;

            nxcip::CompressionType codecID = toNxCodecID(bmiHeader->biCompression);
            if (codecID != targetCodecID)
                continue;

            auto timePerFrame = viHeader->AvgTimePerFrame;
            int maxFps = timePerFrame ? (REFERENCE_TIME) 10000000 / timePerFrame : 0;
            ResolutionData data(bmiHeader->biWidth, bmiHeader->biHeight, maxFps);

            if (std::find(resolutionList.begin(), resolutionList.end(), data) == resolutionList.end())
                resolutionList.push_back(data);
        }
        DeleteMediaType(mediaType);
    }

    *outResolutionList = resolutionList;

    enumMediaTypes->Release();
    return resultCode;
}

HRESULT getBitrateList(IMoniker *pMoniker,
    std::vector<int>* outBitrateList,
    nxcip::CompressionType targetCodecID)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT resultCode = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(resultCode))
        return resultCode;

    std::vector<int> bitrateList;

    AM_MEDIA_TYPE* mediaType = NULL;
    while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
    {
        if ((mediaType->formattype == FORMAT_VideoInfo) &&
            (mediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
            (mediaType->pbFormat != NULL))
        {
            auto viHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);

            nxcip::CompressionType codecID = toNxCodecID(viHeader->bmiHeader.biCompression);
            if (codecID != targetCodecID)
                continue;

            int bitrate = (int) viHeader->dwBitRate;
            if (std::find(bitrateList.begin(), bitrateList.end(), bitrate) == bitrateList.end())
                bitrateList.push_back(bitrate);
        }
        DeleteMediaType(mediaType);
    }

    *outBitrateList = bitrateList;

    enumMediaTypes->Release();
    return resultCode;
}

std::string getDeviceName(IMoniker *pMoniker)
{
    VARIANT var;
    HRESULT resultCode = getDeviceProperty(pMoniker, L"FriendlyName", &var);
    if (FAILED(resultCode))
        return {};
    return toStdString(var.bstrVal);
}

std::string getDevicePath(IMoniker *pMoniker)
{
    VARIANT var;
    HRESULT resultCode = getDeviceProperty(pMoniker, L"DevicePath", &var);
    if (FAILED(resultCode))
        return {};
    return toStdString(var.bstrVal);
}

} // namespace

//-------------------------------------------------------------------------------------------------
// Public API 

std::string getDeviceName(const char * devicePath)
{
    DShowInitializer init;
    IMoniker * pMoniker = NULL;
    HRESULT resultCode = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (SUCCEEDED(resultCode) && pMoniker)
        return getDeviceName(pMoniker);
    return std::string();
}

std::vector<DeviceData> getDeviceList()
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<DeviceData> deviceNames;
    IEnumMoniker * pEnum;
    HRESULT resultCode = enumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
    if (FAILED(resultCode))
        return deviceNames;

    IMoniker *pMoniker = NULL;
    while (S_OK == pEnum->Next(1, &pMoniker, NULL))
    {
        deviceNames.push_back(DeviceData(getDeviceName(pMoniker), getDevicePath(pMoniker)));
        pMoniker->Release();
    }
    pEnum->Release();
    return deviceNames;
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath)
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<nxcip::CompressionType> codecList;

    IMoniker * pMoniker = NULL;
    HRESULT resultCode = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (SUCCEEDED(resultCode) && pMoniker)
        getSupportedCodecs(pMoniker, &codecList);

    if (pMoniker)
        pMoniker->Release();

    return codecList;
}

std::vector<ResolutionData> getResolutionList(const char * devicePath, nxcip::CompressionType targetCodecID)
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<ResolutionData> resolutionList;

    IMoniker * pMoniker = NULL;
    HRESULT resultCode = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (FAILED(resultCode))
        return resolutionList;

    if (pMoniker)
    {
        getResolutionList(pMoniker, &resolutionList, targetCodecID);
        pMoniker->Release();
    }

    return resolutionList;
}

void setBitrate(const char * devicePath, int bitrate)
{
    DShowInitializer init;

}

int getMaxBitrate(const char * devicePath, nxcip::CompressionType targetCodecID)
{
    DShowInitializer init;

    IMoniker * pMoniker = NULL;
    HRESULT resultCode = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (FAILED(resultCode))
        return -1;

    std::vector<int> bitrateList;

    if (pMoniker)
    {
        getBitrateList(pMoniker, &bitrateList, targetCodecID);
        pMoniker->Release();
    }

    int largest = 0;
    for(const auto & bitrate : bitrateList)
    {
        if(largest < bitrate)
            largest = bitrate;
    }

    return largest;
}

} // namespace impl
} // namespace device
} // namespace nx

#endif