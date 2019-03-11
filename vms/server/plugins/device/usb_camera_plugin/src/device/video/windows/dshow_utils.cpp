#ifdef _WIN32

#include "dshow_utils.h"

#include <map>

#include "wdm_utils.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace detail {

namespace {

//-------------------------------------------------------------------------------------------------
// DShowCompressionTypeDescriptor

class DShowCompressionTypeDescriptor : public AbstractCompressionTypeDescriptor
{
public:
    DShowCompressionTypeDescriptor(AM_MEDIA_TYPE * mediaType);
    ~DShowCompressionTypeDescriptor();
    nxcip::CompressionType toNxCompressionType() const override;
    VIDEOINFOHEADER * videoInfoHeader() const;
    BITMAPINFOHEADER *  videoInfoBitMapHeader() const;
private:
    AM_MEDIA_TYPE * m_mediaType;
};

DShowCompressionTypeDescriptor::DShowCompressionTypeDescriptor(AM_MEDIA_TYPE * mediaType):
    m_mediaType(mediaType)
{
}

DShowCompressionTypeDescriptor::~DShowCompressionTypeDescriptor()
{
    if (m_mediaType)
    {
        deleteMediaType(m_mediaType);
        m_mediaType = NULL;
    }
}

nxcip::CompressionType DShowCompressionTypeDescriptor::toNxCompressionType() const
{

        auto viHeader = videoInfoHeader();
        return viHeader
            ? toNxCodecID(viHeader->bmiHeader.biCompression)
            : nxcip::AV_CODEC_ID_NONE;
}

VIDEOINFOHEADER * DShowCompressionTypeDescriptor::videoInfoHeader() const
{
   return isVideoInfo(m_mediaType)
        ? reinterpret_cast<VIDEOINFOHEADER*>(m_mediaType->pbFormat)
        : nullptr;
}

BITMAPINFOHEADER *  DShowCompressionTypeDescriptor::videoInfoBitMapHeader() const
{
    auto videoInfo = videoInfoHeader();
    return videoInfo ? &videoInfo->bmiHeader : nullptr;
}

} // namespace

std::string getDeviceName(const std::string& devicePath)
{
    DShowInitializer init;
    IMoniker * pMoniker = NULL;
    HRESULT result = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (SUCCEEDED(result) && pMoniker)
        return getDeviceName(pMoniker);
    return {};
}

std::vector<DeviceData> getDeviceList()
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<DeviceData> devices;
    IEnumMoniker * pEnum;
    HRESULT result = enumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
    if (FAILED(result))
        return {};

    std::map<std::string, int> duplicateCameras;

    IMoniker *pMoniker = NULL;
    while (S_OK == pEnum->Next(1, &pMoniker, NULL))
    {
        std::string name = getDeviceName(pMoniker);

        int nameIndex = 0;
        auto it = duplicateCameras.find(name);
        if (it == duplicateCameras.end())
            duplicateCameras.emplace(name, 0);
        else
            nameIndex = ++it->second;

        std::string path = getDevicePath(pMoniker);

        // Do not use device serial number, due to some cameras return serial number in enabled
        // state only.
        std::string uniqueId = std::to_string(nameIndex) + "-" + name;

        devices.push_back(DeviceData(name, path, uniqueId));
        pMoniker->Release();
    }
    pEnum->Release();
    return devices;
}

std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const std::string& devicePath)
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<device::CompressionTypeDescriptorPtr> codecList;

    IMoniker * pMoniker = NULL;
    HRESULT result = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (SUCCEEDED(result) && pMoniker)
        getSupportedCodecs(pMoniker, &codecList);

    if (pMoniker)
        pMoniker->Release();

    return codecList;
}

std::vector<ResolutionData> getResolutionList(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodec)
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<ResolutionData> resolutionList;

    IMoniker * pMoniker = NULL;
    HRESULT result = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (FAILED(result))
        return {};

    if (pMoniker)
    {
        getResolutionList(pMoniker, &resolutionList, targetCodec);
        pMoniker->Release();
    }

    return resolutionList;
}

void setBitrate(
    const std::string& devicePath,
    int bitrate,
    const device::CompressionTypeDescriptorPtr& targetCodec)
{
    DShowInitializer init;
    IMoniker * pMoniker = NULL;
    HRESULT result = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (FAILED(result))
        return;

    IBaseFilter * baseFilter = NULL;
    result = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **) &baseFilter);
    if (FAILED(result))
        return;

    IPin * pin = NULL;
    //todo find out if there are multiple output pins for a device. For now we assume only one
    result = getPin(baseFilter, PINDIR_OUTPUT, &pin);
    if (FAILED(result))
        return;

    IAMStreamConfig * pConfig = NULL;
    result = pin->QueryInterface(IID_IAMStreamConfig, (void**) &pConfig);
    if (FAILED(result))
        return;

    int capCount;
    int iSize;
    result = pConfig->GetNumberOfCapabilities(&capCount, &iSize);
    if (FAILED(result))
        return;

    auto dshowCodec = std::dynamic_pointer_cast<DShowCompressionTypeDescriptor>(targetCodec);
    BITMAPINFOHEADER * targetHeader = dshowCodec->videoInfoBitMapHeader();

    BYTE *array = new BYTE[iSize];
    AM_MEDIA_TYPE *mediaType;
    for (int i = 0; i < capCount; ++i)
    {
        result = pConfig->GetStreamCaps(i, &mediaType, array);
        if (SUCCEEDED(result))
        {
            if (isVideoInfo(mediaType) && targetHeader)
            {
                VIDEOINFO * info = reinterpret_cast<VIDEOINFO*>(mediaType->pbFormat);
                if (info->bmiHeader.biCompression == targetHeader->biCompression)
                {
                    info->dwBitRate = bitrate;
                    pConfig->SetFormat(mediaType);
                }
            }
        }
        deleteMediaType(mediaType);
    }

    baseFilter->Release();
    delete[] array;
}

int getMaxBitrate(const std::string& devicePath, const device::CompressionTypeDescriptorPtr& targetCodec)
{
#if 0
    DShowInitializer init;
    IMoniker * pMoniker = NULL;
    HRESULT result = findDevice(CLSID_VideoInputDeviceCategory, devicePath, &pMoniker);
    if (FAILED(result))
        return -1;

    std::vector<int> bitrateList;

    if (pMoniker)
    {
        getBitrateList(pMoniker, &bitrateList, targetCodec);
        pMoniker->Release();
    }

    int largest = 0;
    for (const auto & bitrate : bitrateList)
    {
        if (largest < bitrate)
            largest = bitrate;
    }

    return largest;
#else
    return 0;
#endif
}

//-------------------------------------------------------------------------------------------------
// api helper functions

// borrowed from https://msdn.microsoft.com/en-us/library/windows/desktop/dd375432(v=vs.85).aspx
// Release the format block for a media type.
void freeMediaType(AM_MEDIA_TYPE& mediaType)
{
    if (mediaType.cbFormat != 0)
    {
        CoTaskMemFree((PVOID) mediaType.pbFormat);
        mediaType.cbFormat = 0;
        mediaType.pbFormat = NULL;
    }
    if (mediaType.pUnk != NULL)
    {
        // Unecessary because pUnk should not be used, but safest.
        mediaType.pUnk->Release();
        mediaType.pUnk = NULL;
    }
}

// borrowed from https://msdn.microsoft.com/en-us/library/windows/desktop/dd375432(v=vs.85).aspx
// Delete a media type structure that was allocated on the heap.
void deleteMediaType(AM_MEDIA_TYPE *mediaType)
{
    if (mediaType != NULL)
    {
        freeMediaType(*mediaType);
        CoTaskMemFree(mediaType);
    }
}

bool isVideoInfo(AM_MEDIA_TYPE * mediaType)
{
    return
        mediaType->formattype == FORMAT_VideoInfo &&
        mediaType->cbFormat >= sizeof(VIDEOINFOHEADER) &&
        mediaType->pbFormat != NULL;
}

nxcip::CompressionType toNxCodecID(DWORD biCompression)
{
    switch (biCompression)
    {
        case MAKEFOURCC('H', '2', '6', '3'): return nxcip::AV_CODEC_ID_H263;
        case MAKEFOURCC('H', '2', '6', '4'): return nxcip::AV_CODEC_ID_H264;
        case MAKEFOURCC('M', 'J', 'P', 'G'): return nxcip::AV_CODEC_ID_MJPEG;
        case MAKEFOURCC('M', 'P', 'G', '2'):
        case MAKEFOURCC('M', 'P', '2', 'V'): return nxcip::AV_CODEC_ID_MPEG2VIDEO;
        case MAKEFOURCC('M', 'P', 'G', '4'):
        case MAKEFOURCC('M', 'P', '4', 'V'): return nxcip::AV_CODEC_ID_MPEG4;
        case MAKEFOURCC('T', 'H', 'E', 'O'): return nxcip::AV_CODEC_ID_THEORA;
        case MAKEFOURCC('P', 'N', 'G', ' '): return nxcip::AV_CODEC_ID_PNG;
        case MAKEFOURCC('G', 'I', 'F', ' '): return nxcip::AV_CODEC_ID_GIF;
        default: return nxcip::AV_CODEC_ID_NONE;
    }
}

HRESULT getPin(IBaseFilter *pFilter, PIN_DIRECTION pinDirection, IPin **outPin)
{
    IEnumPins* pEnum = NULL;
    IPin* pPin = NULL;
    HRESULT result;

    if (outPin == NULL)
        return E_POINTER;

    result = pFilter->EnumPins(&pEnum);
    if (FAILED(result))
        return result;

    while (pEnum->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION pinDirectionThis;
        result = pPin->QueryDirection(&pinDirectionThis);
        if (SUCCEEDED(result) && pinDirection == pinDirectionThis)
        {
            // Found a match. Return the IPin pointer to the caller.
            *outPin = pPin;
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
    IPropertyBag *bag = NULL;
    HRESULT result = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &bag);
    if (FAILED(result))
        return result;

    VariantInit(outVar);
    result = bag->Read(propName, outVar, 0);
    bag->Release();
    return result;
}

HRESULT setDeviceProperty(IMoniker * pMoniker, LPCOLESTR propName, VARIANT * outVar)
{
    IPropertyBag *bag = NULL;
    HRESULT result = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &bag);
    if (FAILED(result))
        return result;

    VariantInit(outVar);
    result = bag->Write(propName, outVar);
    bag->Release();
    return result;
}

HRESULT enumerateMediaTypes(
    IMoniker* pMoniker,
    IEnumMediaTypes ** outEnumMediaTypes,
    IPin** outPin)
{
    *outEnumMediaTypes = NULL;

    IBaseFilter * baseFilter = NULL;
    HRESULT result = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **) &baseFilter);
    if (FAILED(result))
        return result;

    IPin * pin = NULL;
    //todo find out if there are multiple output pins for a device. For now we assume only one
    result = getPin(baseFilter, PINDIR_OUTPUT, &pin);
    if (SUCCEEDED(result))
    {
        IEnumMediaTypes * enumMediaTypes = NULL;
        result = pin->EnumMediaTypes(&enumMediaTypes);
        if (SUCCEEDED(result))
            *outEnumMediaTypes = enumMediaTypes;

        if (outPin)
            *outPin = pin;
        else
           pin->Release();
    }
    baseFilter->Release();
    return result;
}

HRESULT enumerateDevices(REFGUID category, IEnumMoniker ** ppEnum)
{
    // Create the System Device Enumerator.
    ICreateDevEnum *pDevEnum;
    HRESULT result = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(result))
    {
        // Create an enumerator for the category.
        result = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (result == S_FALSE)
            result = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.

        pDevEnum->Release();
    }
    return result;
}

HRESULT findDevice(REFGUID category, const std::string& devicePath, IMoniker ** outMoniker)
{
    IEnumMoniker* pEnum = NULL;
    HRESULT result = enumerateDevices(category, &pEnum);
    if (FAILED(result))
        return result;

    CComBSTR devicePathBstr(devicePath.c_str());

    IMoniker* pMoniker = NULL;
    while (S_OK == (result = pEnum->Next(1, &pMoniker, NULL)))
    {
        VARIANT var;
        result = getDeviceProperty(pMoniker, L"DevicePath", &var);
        if (FAILED(result))
            return result;

        // don't release the moniker in this case, it's the one we are looking for
        if (wcscmp(var.bstrVal, devicePathBstr) == 0)
        {
            pEnum->Release();
            *outMoniker = pMoniker;
            return result;
        }
        pMoniker->Release();
    }

    pEnum->Release();
    *outMoniker = NULL;
    return result;
}

HRESULT getSupportedCodecs(
    IMoniker *pMoniker,
    std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> *outCodecList)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT result = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(result))
        return result;

    std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> codecList;

    AM_MEDIA_TYPE* mediaType = NULL;
    while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
    {
        std::shared_ptr<AbstractCompressionTypeDescriptor> descriptor;
        if (isVideoInfo(mediaType))
        {
            descriptor = std::make_shared<DShowCompressionTypeDescriptor>(mediaType);
            codecList.push_back(descriptor);
        }
        if (!descriptor)
            deleteMediaType(mediaType);
    }

    *outCodecList = codecList;
    enumMediaTypes->Release();
    return result;
}

HRESULT getResolutionList(IMoniker *pMoniker,
    std::vector<ResolutionData>* outResolutionList,
    const device::CompressionTypeDescriptorPtr& targetCodec)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT result = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(result))
        return result;

    std::vector<ResolutionData> resolutionList;
    auto dshowDescriptor = std::dynamic_pointer_cast<DShowCompressionTypeDescriptor>(targetCodec);

    AM_MEDIA_TYPE* mediaType = NULL;
    while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
    {
        if (isVideoInfo(mediaType))
        {
            auto viHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
            BITMAPINFOHEADER *bmiHeader = &viHeader->bmiHeader;

            BITMAPINFOHEADER * targetBitMapInfo = dshowDescriptor->videoInfoBitMapHeader();

            if (bmiHeader->biCompression == targetBitMapInfo->biCompression)
            {
                if (viHeader->AvgTimePerFrame)
                {
                    float fps = static_cast<float>(10000000 / viHeader->AvgTimePerFrame);
                    resolutionList.push_back(
                        ResolutionData(bmiHeader->biWidth, bmiHeader->biHeight, fps));
                }
            }
        }
        deleteMediaType(mediaType);
    }

    *outResolutionList = resolutionList;

    enumMediaTypes->Release();
    return result;
}

HRESULT getBitrateList(IMoniker *pMoniker,
    std::vector<int>* outBitrateList,
    const device::CompressionTypeDescriptorPtr& targetCodec)
{
    IEnumMediaTypes * enumMediaTypes = NULL;
    HRESULT result = enumerateMediaTypes(pMoniker, &enumMediaTypes);
    if (FAILED(result))
        return result;

    std::vector<int> bitrateList;

    auto dshowCodec = std::dynamic_pointer_cast<DShowCompressionTypeDescriptor>(targetCodec);
    BITMAPINFOHEADER * targetHeader = dshowCodec->videoInfoBitMapHeader();

    AM_MEDIA_TYPE* mediaType = NULL;
    while (S_OK == enumMediaTypes->Next(1, &mediaType, NULL))
    {
        if (isVideoInfo(mediaType) && targetHeader)
        {
            auto viHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);

            if (viHeader->bmiHeader.biCompression == targetHeader->biCompression)
            {
                int bitrate = (int) viHeader->dwBitRate;
                if (std::find(
                        bitrateList.begin(),
                        bitrateList.end(),
                        bitrate) == bitrateList.end())
                    {
                        bitrateList.push_back(bitrate);
                    }
            }
        }
        deleteMediaType(mediaType);
    }

    *outBitrateList = bitrateList;

    enumMediaTypes->Release();
    return result;
}

std::string toStdString(BSTR bstr)
{
    char * ch = _com_util::ConvertBSTRToString(bstr);
    std::string str;
    if (ch)
    {
        str = ch;
        delete ch;
    }
    return str;
}

std::string getDeviceName(IMoniker *pMoniker)
{
    VARIANT var;
    HRESULT result = getDeviceProperty(pMoniker, L"FriendlyName", &var);
    if (FAILED(result))
        return {};
    return toStdString(var.bstrVal);
}

std::string getDevicePath(IMoniker *pMoniker)
{
    VARIANT var;
    HRESULT result = getDeviceProperty(pMoniker, L"DevicePath", &var);
    if (FAILED(result))
        return {};
    return toStdString(var.bstrVal);
}

//--------------------------------------------------------------------------------------------------
// audio

LONG getWaveInID(IMoniker * pMoniker)
{
    VARIANT var;
    HRESULT result = getDeviceProperty(pMoniker, L"WaveInID", &var);
    if (FAILED(result))
        return {};
    return var.lVal;
}

std::string getDisplayName(IMoniker * pMoniker)
{
    LPOLESTR displayName = NULL;
    HRESULT result = pMoniker->GetDisplayName(NULL, NULL, &displayName);
    if (FAILED(result))
        return {};

    USES_CONVERSION;
    LPCSTR str = OLE2CA(displayName);
    return std::string(str);
}

std::vector<AudioDeviceDescriptor> getAudioDeviceList()
{
    //todo figure out how to avoid this init everytime
    DShowInitializer init;

    std::vector<AudioDeviceDescriptor> devices;
    IEnumMoniker * pEnum;
    HRESULT result = enumerateDevices(CLSID_AudioInputDeviceCategory, &pEnum);
    if (FAILED(result))
        return devices;

    IMoniker *pMoniker = NULL;
    while (S_OK == pEnum->Next(1, &pMoniker, NULL))
    {
        // Intentionally using getDisplayName, it returns the device's instance id
        std::string devicePath = getDisplayName(pMoniker);
        DeviceData data(getDeviceName(pMoniker), devicePath, devicePath);
        devices.push_back(AudioDeviceDescriptor(data, getWaveInID(pMoniker)));
        pMoniker->Release();
    }
    pEnum->Release();
    return devices;
}

} // namespace detail
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif
