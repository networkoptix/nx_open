#ifdef _WIN32

#include "wdm_utils.h"

#include <vector>
#include <algorithm>

#include <usb.h>
#include <initguid.h>
#include <usbioctl.h>
#include <cfgmgr32.h>
#include <setupapi.h>

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace detail {

namespace {

static const GUID kCaptureGuid = 
{
    0x65e8773d, // Data1
    0x8f56, // Data2
    0x11d0, // Data3
    {0xa3, 0xb9, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96} // Data4[8]
};

static const std::wstring kUsbDeviceClassGuid(L"A5DCBF10-6530-11D2-901F-00C04FB951ED");
static const std::wstring kUsbHubClassGuid(L"f18a0e88-c30c-11d0-8815-00a0c906bed8");

std::wstring toWString(const std::string& str)
{
    if(str.empty())
        return std::wstring();

    std::size_t size = std::mbstowcs(NULL, str.c_str(), 0) + 1; //< + 1 for NULL terminator.
    std::vector<wchar_t> buffer(size);
    std::mbstowcs(&buffer[0], str.c_str(), size);

    return std::wstring(&buffer[0]);
}

std::string toString(const std::wstring& wideStr)
{
    if(wideStr.empty())
        return std::string();

    std::size_t size = std::wcstombs(NULL, wideStr.c_str(), 0) + 1; //< + 1 for NULL terminator.
    std::vector<char> buffer(size);
    std::wcstombs(&buffer[0], wideStr.c_str(), size);

    return std::string(&buffer[0]);
}

static std::wstring toDevicePath(
    const std::wstring& deviceInstanceId,
    const std::wstring & classGuid)
{
    static const std::wstring kPrefix(L"\\\\?\\");

    std::wstring wstr = deviceInstanceId;
    std::replace(wstr.begin(), wstr.end(), L'\\', L'#');

    return kPrefix + wstr + L"#{" + classGuid + L"}";
}

std::wstring getDevicePath(
    HDEVINFO deviceInfoSet,
    SP_DEVINFO_DATA& deviceData,
    SP_DEVICE_INTERFACE_DATA& interfaceData)
{
    DWORD bufferSize = 0;

    // Call it once to get bufferSize for detailData->DevicePath.
    // It Should return FALSE with ERROR_INSUFFICIENT_BUFFER as error code in this case.
    if (!SetupDiGetDeviceInterfaceDetail(
        deviceInfoSet,
        &interfaceData,
        NULL,
        0,
        &bufferSize,
        &deviceData))
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return std::wstring();
    }

    // SetupDiGetDeviceInterfaceDetail requires space equal to bufferSize returned from
    // the first call for DevicePath + size of the struct + size of an extra TCHAR
    DWORD detailDataSize =
        (bufferSize * sizeof(WCHAR)) + sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + sizeof(TCHAR);    

    // Needs to be dynamically allocaed to give enough space to DevicePath member variable.
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
        (PSP_DEVICE_INTERFACE_DETAIL_DATA) alloca(detailDataSize);

    // cbSize should always the size of the data structure.
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    // Call again to get the actual buffer in detailData->DevicePath.
    if (!SetupDiGetDeviceInterfaceDetail(
        deviceInfoSet,
        &interfaceData,
        detailData,
        detailDataSize,
        0,
        &deviceData))
    {
        return std::wstring();
    }

    return std::wstring(detailData->DevicePath);
}

static bool findDevice(
    const std::wstring& devicePath,
    HDEVINFO deviceInfoSet,
    const GUID * classGuid,
    PSP_DEVINFO_DATA outTargetDevice)
{
    outTargetDevice->cbSize = sizeof(SP_DEVINFO_DATA);

    SP_DEVICE_INTERFACE_DATA interfaceData = {0};
    interfaceData.cbSize = sizeof(interfaceData);

    for (int deviceIndex = 0; 
        SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, outTargetDevice);
        ++deviceIndex)
    {
        for(int interfaceIndex = 0;
            SetupDiEnumDeviceInterfaces(
                deviceInfoSet, 
                outTargetDevice,
                classGuid,
                interfaceIndex,
                &interfaceData);
            ++interfaceIndex)
        {
            std::wstring currentDevicePath = getDevicePath(
                deviceInfoSet,
                *outTargetDevice,
                interfaceData);
            
            size_t size = std::min(currentDevicePath.size(), devicePath.size());
            if (_wcsnicmp(currentDevicePath.c_str(), devicePath.c_str(), size) == 0)
                return true;
        }
    }

    return false;
}

static bool getParentDevice(
    HDEVINFO deviceTree,
    SP_DEVINFO_DATA& childDevice,
    PSP_DEVINFO_DATA outParentDevice,
    std::wstring* outParentDeviceInstanceId = nullptr)
{
    outParentDevice->cbSize = sizeof(SP_DEVINFO_DATA);

    DEVINST parentDeviceNode = 0;
    if (CM_Get_Parent(&parentDeviceNode, childDevice.DevInst, 0) != CR_SUCCESS)
        return false;

    ULONG bufferSize = 0;
    if (CM_Get_Device_ID_Size(&bufferSize, parentDeviceNode, 0) != CR_SUCCESS)
        return false;

    // Make room a Unicode string with a NULL character at the end.
    bufferSize = (bufferSize + 1) * sizeof(WCHAR);

    // Get the parent device's instance id.
    PWSTR buffer = (PWSTR)alloca(bufferSize);
    if (CM_Get_Device_ID(parentDeviceNode, buffer, bufferSize, 0) != CR_SUCCESS)
        return false;

    // Get the actual parent device.
    if (!SetupDiOpenDeviceInfo(deviceTree, buffer, NULL, 0, outParentDevice))
        return false;

    if (outParentDeviceInstanceId)
        *outParentDeviceInstanceId = buffer;

    return true;
}

static LONG getUsbPortIndex(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& targetUsbDevice)
{
    DWORD bufferSize = 0;

    // Call it once to get required buffer size...
    if (!SetupDiGetDeviceRegistryProperty(
        deviceInfoSet,
        &targetUsbDevice, 
        SPDRP_LOCATION_INFORMATION,
        NULL, 
        NULL,
        0,
        &bufferSize))
    {
        // ERROR_INSUFFICIENT_BUFFER is expected: we want the size of buffer. If not, something
        // went wrong.
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return -1;
    }

    std::vector<BYTE> buffer(++bufferSize); //< ++ in front to make room for NULL terminator.

    // And again to fill the buffer.
    // Note: this returns a unicode string, two bytes per character.
    if (!SetupDiGetDeviceRegistryProperty(
        deviceInfoSet,
        &targetUsbDevice,
        SPDRP_LOCATION_INFORMATION,
        NULL,
        &buffer[0],
        bufferSize,
        NULL))
    {
        return -1;
    }

    // The string is expected to have a string of the form "Port_#0001.Hub_#0001".
    std::wstring registryProperty(reinterpret_cast<wchar_t *>(&buffer[0]));

    static const std::wstring kPortPrefix(L"Port_#");

    size_t start = registryProperty.find(kPortPrefix);
    size_t end = registryProperty.find(L".");
   
    if(start == std::wstring::npos || end == std::wstring::npos || end < start)
        return -1;

    start += kPortPrefix.size(); //< Advance past "Port_#".
    if (start >= registryProperty.size() || end < start)
        return -1;

    // Dropping "Port_#" and ".Hub_#0001"
    std::wstring portIndex = registryProperty.substr(start, end - start);
    
    return std::stol(portIndex);
}

static std::string getStringDescriptor(HANDLE usbHubHandle, ULONG portIndex, UCHAR descriptorIndex)
{
    // MAXIMUM_USB_STRING_LENGTH: Allocating additional space for the string inside the request.
    DWORD size = sizeof(PUSB_STRING_DESCRIPTOR) + MAXIMUM_USB_STRING_LENGTH;

    PUSB_DESCRIPTOR_REQUEST request = (PUSB_DESCRIPTOR_REQUEST)alloca(size);
    memset(request, 0, size);

    request->ConnectionIndex = portIndex;
    request->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8) | descriptorIndex;
    request->SetupPacket.wIndex = 0; // < default Language id
    request->SetupPacket.wLength = MAXIMUM_USB_STRING_LENGTH;

    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        usbHubHandle,
        IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
        request,
        size,
        request,
        size,
        &bytesReturned,
        NULL))
    {
        return std::string();
    }

    if (!bytesReturned)
        return std::string();

    PUSB_STRING_DESCRIPTOR theString = (PUSB_STRING_DESCRIPTOR)request->Data;

    return toString(theString->bString);
}

static std::string getUsbSerialNumber(HANDLE usbHubHandle, ULONG portIndex)
{
    USB_NODE_CONNECTION_INFORMATION connectionInfo = {0};
    ULONG size = sizeof(connectionInfo);
    connectionInfo.ConnectionIndex = portIndex;

    ULONG bytesReturned = 0;
    if(!DeviceIoControl(
        usbHubHandle,
        IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, 
        &connectionInfo,
        size,
        &connectionInfo,
        size,
        &bytesReturned,
        NULL))
    {
        return std::string();
    }

    if (!bytesReturned)
        return std::string();

    PUSB_DEVICE_DESCRIPTOR usbDescriptor = &connectionInfo.DeviceDescriptor;

    // Not all usb devices have a serial number.
    if (!usbDescriptor->iSerialNumber)
        return std::string();

    // If it has a serial number, get its string representation
    return getStringDescriptor(usbHubHandle, portIndex, usbDescriptor->iSerialNumber);
}

static bool containsCaptureClassGuid(const std::string& devicePath)
{
    static const std::string kCaptureClassGuid("65e8773d-8f56-11d0-a3b9-00a0c9223196");
    return devicePath.find(kCaptureClassGuid) != std::string::npos;
}

} // namespace

std::string getDeviceUniqueId(const std::string& devicePath)
{
    // Don't bother if the device path provided is not a KSCATEGORY_CAPTURE class device path.
    if (!containsCaptureClassGuid(devicePath))
        return std::string();

    // The algorithm for getting the USB_DEVICE_DESCRIPTOR struct, needed to get the serial number
    // for the target usb device, was adapted from:
    // https://stackoverflow.com/questions/28007468/how-do-i-obtain-usb-device-descriptor-given-a-device-path

    // 1. Locate the target usb device in the set corresponding to devicePath
    HDEVINFO captureDeviceInfoSet = SetupDiGetClassDevs(
        &kCaptureGuid,
        NULL,
        NULL,
        (DIGCF_PRESENT | DIGCF_PROFILE | DIGCF_DEVICEINTERFACE));
    if (!captureDeviceInfoSet)
        return std::string();

    SP_DEVINFO_DATA targetCaptureDevice = {0};
    if (!findDevice(
        toWString(devicePath),
        captureDeviceInfoSet,
        &kCaptureGuid,
        &targetCaptureDevice))
    {
        return std::string();
    }

    HDEVINFO deviceTree = SetupDiCreateDeviceInfoList(NULL, NULL);
    if (deviceTree == INVALID_HANDLE_VALUE)
        return std::string();

    // 2. Translate the capture class device path to its parent usb device.
    SP_DEVINFO_DATA targetUsbDevice = {0};
    if (!getParentDevice(deviceTree, targetCaptureDevice, &targetUsbDevice))
        return std::string();

    // 3. Get the usb hub parent device of the target usb device and its instance Id.
    std::wstring usbHubInstanceId;
    SP_DEVINFO_DATA usbHubDevice = {0};
    if (!getParentDevice(deviceTree, targetUsbDevice, &usbHubDevice, &usbHubInstanceId))
        return std::string();

    std::wstring usbHubDevicePath = toDevicePath(usbHubInstanceId, kUsbHubClassGuid);

    // 4. Get a handle on the parent usb hub device
    HANDLE usbHubHandle = CreateFile(
        usbHubDevicePath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, 
        0, 
        NULL);
    if (usbHubHandle == INVALID_HANDLE_VALUE)
        return std::string();

     // 5. Get the physical port index for the target usb device (not the capture device).
     // The capture device does not have its physical location as a property.
     LONG portIndex = getUsbPortIndex(deviceTree, targetUsbDevice);
     if (portIndex == -1)
         return std::string();

    // 6. Get the serial number
    std::string serialNumber = getUsbSerialNumber(usbHubHandle, portIndex);

    CloseHandle(usbHubHandle);
    SetupDiDestroyDeviceInfoList(captureDeviceInfoSet);
    SetupDiDestroyDeviceInfoList(deviceTree);

    return serialNumber;
}

} //namespace detail
} //namespace video
} //namespace device
} //namespace usb_cam
} //namespace nx

#endif // _WIN32
