#include "dshow_utils.h"

#ifdef _WIN32

#include <dshow.h>
#include <windows.h>

namespace dshow{
namespace utils{


    /**
    * shamelessly borrowed from: https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=651
    */
    QList<QString> listDevices()
    {
        QList<QString> deviceNames;
        ICreateDevEnum *pDevEnum;
        HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**) &pDevEnum);
        if (FAILED(hr))
            return deviceNames;

        IEnumMoniker *pEnum;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (FAILED(hr))
            return deviceNames;

        IMoniker *pMoniker = NULL;
        while (S_OK == pEnum->Next(1, &pMoniker, NULL))
        {
            IPropertyBag *pPropBag;
            LPOLESTR str = 0;
            hr = pMoniker->GetDisplayName(0, 0, &str);
            if (SUCCEEDED(hr))
            {
                hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &pPropBag);

                if (SUCCEEDED(hr))
                {
                    VARIANT var;
                    VariantInit(&var);
                    hr = pPropBag->Read(L"FriendlyName", &var, 0);
                    QString fName = QString::fromWCharArray(var.bstrVal);
                    deviceNames.append(fName);
                }
            }

            IBaseFilter * baseFilter = NULL;
            hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)&baseFilter);
            
            IPin * outPin = NULL;
            if (getPin(baseFilter, PINDIR_OUTPUT, &outPin) != S_OK)
                continue;

            IEnumMediaTypes * enumMediaTypes = NULL;
            if (outPin->EnumMediaTypes(&enumMediaTypes) != S_OK)
                continue;

            AM_MEDIA_TYPE* mediaType = NULL;
            VIDEOINFOHEADER* videoInfoHeader = NULL;
            while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
            {
                if ((mediaType->formattype == FORMAT_VideoInfo) &&
                    (mediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
                    (mediaType->pbFormat != NULL))
                {
                    videoInfoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                    videoInfoHeader->bmiHeader.biWidth;  // Supported width
                    videoInfoHeader->bmiHeader.biHeight; // Supported height
                }
                FreeMediaType(*mediaType);
}
        }
        return deviceNames;
    }

    QList<QSize> listResolutions(IEnumMoniker * deviceMoniker)
    {
        QList<QSize> deviceResolutions;
        //ICreateDevEnum *pDevEnum;
        //HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**) &pDevEnum);
        //if (FAILED(hr))
        //    return deviceResolutions;

        //
        //IEnumMediaTypes* mediaTypesEnumerator = NULL;
        //pDevEnum->CreateClassEnumerator(CLSID_SystemDeviceEnum, &mediaTypesEnumerator, 0);

        //AM_MEDIA_TYPE* mediaType = NULL;
        //VIDEOINFOHEADER* videoInfoHeader = NULL;
        //while (S_OK == mediaTypesEnumerator->Next(1, &mediaType, NULL))
        //{
        //    if ((mediaType->formattype == FORMAT_VideoInfo) &&
        //        (mediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
        //        (mediaType->pbFormat != NULL))
        //    {
        //        videoInfoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
        //        videoInfoHeader->bmiHeader.biWidth;  // Supported width
        //        videoInfoHeader->bmiHeader.biHeight; // Supported height
        //    }
        //    FreeMediaType(*mediaType);
        //}


        IBaseFilter * baseFilter = NULL;
        HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IGraphBuilder, (void **)&baseFilter);
        if (FAILED(hr))
            return deviceResolutions;

        IPin * outPin = NULL;
        getPin(baseFilter, PINDIR_OUTPUT, &outPin);



    }

    HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
    {
        IEnumPins  *pEnum = NULL;
        IPin       *pPin = NULL;
        HRESULT    hr;

        if (ppPin == NULL)
        {
            return E_POINTER;
        }

        hr = pFilter->EnumPins(&pEnum);
        if (FAILED(hr))
        {
            return hr;
        }
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


} // namespace utils
} // namespace dshow

#endif