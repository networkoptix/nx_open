#ifdef _WIN32

#include "dshow_utils.h"

#include <dshow.h>
#include <windows.h>
#include <comip.h>
#include <atlmem.h>
#include <ATLComMem.h>

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace dshow {

//-------------------------------------------------------------------------------------------------
// Private helper functions

void FreeMediaType(AM_MEDIA_TYPE& mt);
void DeleteMediaType(AM_MEDIA_TYPE *pmt);

HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin);
HRESULT getDeviceProperty(IMoniker * pMoniker, LPCOLESTR propName, VARIANT * outVar);
HRESULT enumerateDevices(REFGUID category, IEnumMoniker **ppEnum);
HRESULT findDevice(REFGUID category, const char * devicePath, IMoniker ** outMoniker);

HRESULT getSupportedCodecs(IMoniker *pMoniker, std::vector<nxcip::CompressionType>* codecList);
HRESULT getResolutionList(IMoniker *pMoniker, 
    std::vector<ResolutionData>* outResolutionList, 
    nxcip::CompressionType codecID);

std::string toStdString(BSTR str);
nxcip::CompressionType toNxCodecID(DWORD biCompression);
    
HRESULT getDeviceName(IMoniker *pMoniker, DeviceData *outDeviceInfo);
HRESULT getDevicePath(IMoniker *pMoniker, DeviceData *outDeviceInfo);
HRESULT getResolutionList(IMoniker *pMoniker,
    DeviceData *outDeviceInfo,
    nxcip::CompressionType codecID);


// initializes dshow for the thread that calls the public util functions
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms695279(v=vs.85).aspx
// note: only use this from the public api functions listed in "dshow_utils.h"
// todo: This is a work around, we shouldn't have to init and deinit every time the util 
//       functions are called.
class DShowInitializer
{
public:
    DShowInitializer()
    {
        m_result = CoInitialize(NULL);
    }

    ~DShowInitializer()
    {
        if(S_OK == m_result)
            CoUninitialize();
    }
private:
    HRESULT m_result;
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
            //these are all the formats supported by the webcam plugingetR
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
        CoTaskMemFree((PVOID)mt.pbFormat);
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

HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
{
    IEnumPins  *pEnum = NULL;
    IPin       *pPin = NULL;
    HRESULT    hr;

    if (ppPin == NULL)
        return E_POINTER;

    hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
        return hr;

    while(pEnum->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION PinDirThis;
        hr = pPin->QueryDirection(&PinDirThis);
        if (FAILED(hr))
        {
            pPin->Release();
            pEnum->Release();
            return hr;
        }
        if (PinDir == PinDirThis)
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
    IPropertyBag *pPropBag;
    HRESULT hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &pPropBag);
    if (FAILED(hr))
        return hr;

    VariantInit(outVar);
    hr = pPropBag->Read(propName, outVar, 0);
    pPropBag->Release();
    return hr;
}

HRESULT enumerateMediaTypes(IMoniker* pMoniker, IEnumMediaTypes ** outEnumMediaTypes)
{
    *outEnumMediaTypes = NULL;

    IBaseFilter * baseFilter = NULL;
    HRESULT hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **) &baseFilter);
    if (FAILED(hr))
        return hr;

    IPin * pin = NULL;
    //todo find out if there are multiple output pins for a device. For now we assume only one
    hr = getPin(baseFilter, PINDIR_OUTPUT, &pin);
    if (SUCCEEDED(hr))
    {
        IEnumMediaTypes * enumMediaTypes = NULL;
        hr = pin->EnumMediaTypes(&enumMediaTypes);
        if (SUCCEEDED(hr))
            *outEnumMediaTypes = enumMediaTypes;
        pin->Release();
    }
    baseFilter->Release();
    return hr;
}

HRESULT enumerateDevices(REFGUID category, IEnumMoniker ** ppEnum)
{
    // Create the System Device Enumerator.
    ICreateDevEnum *pDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,  
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr))
    {
        // Create an enumerator for the category.
        hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (hr == S_FALSE)
            hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.

        pDevEnum->Release();
    }
    return hr;
}

HRESULT findDevice(REFGUID category, const char * devicePath, IMoniker ** outMoniker)
{
    IEnumMoniker * pEnum;
    HRESULT hr = enumerateDevices(category, &pEnum);
    if (FAILED(hr))
        return hr;

    CComBSTR devPathBstr(devicePath);
   
    IMoniker *pMoniker = NULL;
    while (S_OK == (hr = pEnum->Next(1, &pMoniker, NULL)))
    {
        VARIANT var;    
        getDeviceProperty(pMoniker, L"DevicePath", &var);
        
        // don't release the moniker in this case, it's the one we are looking for
        if (wcscmp(var.bstrVal, devPathBstr) == 0)
        {
            pEnum->Release();
            *outMoniker = pMoniker;
            return hr;
        }
        pMoniker->Release();
    }

    pEnum->Release();
    *outMoniker = NULL;
    return hr;
}

HRESULT getSupportedCodecs(IMoniker *pMoniker, std::vector<nxcip::CompressionType> *outCodecList)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT hr = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(hr))
        return hr;

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
    return hr;
}

HRESULT getResolutionList(IMoniker *pMoniker,
    std::vector<ResolutionData>* outResolutionList,
    nxcip::CompressionType targetCodecID)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT hr = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(hr))
        return hr;

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

            ResolutionData resData;
            resData.resolution = nxcip::Resolution(bmiHeader->biWidth, bmiHeader->biHeight);
            resData.codecID = codecID;

            if(std::find(resolutionList.begin(), resolutionList.end(), resData) == resolutionList.end())
                resolutionList.push_back(resData);
        }
        DeleteMediaType(mediaType);
    }

    *outResolutionList = resolutionList;

    enumMediaTypes->Release();
    return hr;
}

HRESULT getDeviceName(IMoniker *pMoniker, DeviceData *outDeviceInfo)
{
    VARIANT var;
    HRESULT hr = getDeviceProperty(pMoniker, L"FriendlyName", &var);
    if (FAILED(hr))
        return hr;

    outDeviceInfo->setDeviceName(toStdString(var.bstrVal));

    return hr;
}

HRESULT getDevicePath(IMoniker *pMoniker, DeviceData *outDeviceInfo)
{
    VARIANT var;
    HRESULT hr = getDeviceProperty(pMoniker, L"DevicePath", &var);
    if (FAILED(hr))
        return hr;

    outDeviceInfo->setDevicePath(toStdString(var.bstrVal));

    return hr;
}

HRESULT getResolutionList(IMoniker *pMoniker, DeviceData *outDeviceInfo, nxcip::CompressionType targetCodecID)
{
    std::vector<ResolutionData> resolutionList;
    HRESULT hr = getResolutionList(pMoniker, &resolutionList, targetCodecID);
    if (FAILED(hr))
        return hr;

    outDeviceInfo->setResolutionList(resolutionList);
}

//-------------------------------------------------------------------------------------------------
// Public API 

std::vector<DeviceData> getDeviceList(bool getResolution, nxcip::CompressionType targetCodecID)
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<DeviceData> deviceNames;
    IEnumMoniker * pEnum;
    HRESULT hr = enumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
    if (FAILED(hr))
        return deviceNames;

    IMoniker *pMoniker = NULL;
    while (S_OK == pEnum->Next(1, &pMoniker, NULL))
    {
        DeviceData deviceInfo;
        getDeviceName(pMoniker, &deviceInfo);
        getDevicePath(pMoniker, &deviceInfo);
        if (getResolution)
            getResolutionList(pMoniker, &deviceInfo, targetCodecID);
        deviceNames.push_back(deviceInfo);
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
    HRESULT hr = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (SUCCEEDED(hr) && pMoniker)
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
    HRESULT hr = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (FAILED(hr))
        return resolutionList;

    if (pMoniker)
    {
        getResolutionList(pMoniker, &resolutionList, targetCodecID);
        pMoniker->Release();
    }

    return resolutionList;
}

} // namespace dshow
} // namespace utils
} // namespace webcam_plugin
} // namespace nx
#endif