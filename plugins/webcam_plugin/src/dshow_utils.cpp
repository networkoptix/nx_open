#include "dshow_utils.h"

#ifdef _WIN32

#include <dshow.h>
#include <windows.h>

#include <thread>

namespace utils{
namespace dshow{

    class DShowInitializer
    {
    public:
        DShowInitializer(COINIT coinit = COINIT_APARTMENTTHREADED)
        {
            CoInitializeEx(NULL, coinit);
        }

        ~DShowInitializer()
        {
            CoUninitialize();
        }
    };

    void FreeMediaType(AM_MEDIA_TYPE& mt);
    void DeleteMediaType(AM_MEDIA_TYPE *pmt);
    HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin);
    HRESULT getDeviceName(IMoniker *pMoniker, DeviceInfo *outDeviceInfo);
    HRESULT getResolutionList(IMoniker *pMoniker, DeviceInfo *outDeviceInfo);
    HRESULT enumerateDevices(REFGUID category, IEnumMoniker **ppEnum);

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


    HRESULT getDeviceName(IMoniker *pMoniker, DeviceInfo *outDeviceInfo)
    {
        IPropertyBag *pPropBag;
        LPOLESTR str = 0;
        HRESULT hr = pMoniker->GetDisplayName(0, 0, &str);
        if (FAILED(hr))
            return hr;
            
        hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &pPropBag);
        if (FAILED(hr))
            return hr;

        VARIANT var;
        VariantInit(&var);
        //hr = pPropBag->Read(L"FriendlyName", &var, 0);
        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        outDeviceInfo->setDeviceName(QString::fromWCharArray(var.bstrVal));
        return hr;
    }

    HRESULT getResolutionList(IMoniker *pMoniker, DeviceInfo *outDeviceInfo)
    {
        IBaseFilter * baseFilter = NULL;
        HRESULT hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **) &baseFilter);
        if (FAILED(hr))
            return hr;

        IPin * pin = NULL;
        //todo find out if there are multiple output pins for a device. For now we assume only one
        hr = getPin(baseFilter, PINDIR_OUTPUT, &pin);
        if(FAILED(hr))
            return hr;

        IEnumMediaTypes * enumMediaTypes = NULL;
        hr = pin->EnumMediaTypes(&enumMediaTypes);
        if(FAILED(hr))
            return hr;

        QList<QSize> resolutionList;

        AM_MEDIA_TYPE* mediaType = NULL;
        VIDEOINFOHEADER* videoInfoHeader = NULL;
        while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
        {
            if ((mediaType->formattype == FORMAT_VideoInfo) &&
                (mediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
                (mediaType->pbFormat != NULL))
            {
                videoInfoHeader = (VIDEOINFOHEADER*) mediaType->pbFormat;
                QSize resolution(
                    videoInfoHeader->bmiHeader.biWidth,
                    videoInfoHeader->bmiHeader.biHeight);

                
                if(!resolutionList.contains(resolution))
                    resolutionList.append(resolution);
            }
            DeleteMediaType(mediaType);

        }

        outDeviceInfo->setResolutionList(resolutionList);

        enumMediaTypes->Release();
        pin->Release();
        baseFilter->Release();
        return hr;
    }

    HRESULT enumerateDevices(REFGUID category, IEnumMoniker ** ppEnum)
    {
        //todo figure out how to avoid this init everytime
        DShowInitializer init;

        // Create the System Device Enumerator.
        ICreateDevEnum *pDevEnum;
        HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,  
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

        if (FAILED(hr))
            return hr;

        // Create an enumerator for the category.
        hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (hr == S_FALSE)
            hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.

        pDevEnum->Release();
    }

    /**
    * borrowed from: https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=651
    */
    QList<DeviceInfo> listDevices()
    {
        QList<DeviceInfo> deviceNames;

        IEnumMoniker * pEnum;
        HRESULT hr = enumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
        if (FAILED(hr))
            return deviceNames;

        IMoniker *pMoniker = NULL;
        while (S_OK == pEnum->Next(1, &pMoniker, NULL))
        {
            DeviceInfo deviceInfo;
            getDeviceName(pMoniker, &deviceInfo);
            getResolutionList(pMoniker, &deviceInfo);
            deviceNames.append(deviceInfo);
        }

        return deviceNames;
    }


} // namespace dshow
} // namespace utils

#endif