#include "targetver.h"

#include <vector>
#include <iostream>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

#include <atlbase.h>
#include <atlstr.h>

#include <comdef.h>
#include <Wbemidl.h>

#include <string>
#include <sstream>
#include <iostream>
#include <set>

#include <QtCore/QList>
#include <QtCore/QByteArray>

#include "util.h"
#include "hardware_id.h"
#include "api/global_settings.h"

#define _WIN32_DCOM
# pragma comment(lib, "wbemuuid.lib")

namespace {

struct Device {
    std::string xclass;
    std::string mac;
};

void execQuery1(IWbemServices *pSvc, const BSTR fieldName, const BSTR objectName, bstr_t& rezStr)
{
    rezStr = _T("");

    bstr_t reqStr = bstr_t(_T("SELECT ")) + fieldName + _T(" FROM ") + objectName;

    HRESULT hres;

    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _T("WQL"), 
        reqStr,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        std::ostringstream os;
        os << "Query for operating system name failed. Error code = 0x"
            << " " << std::ios_base::hex << hres;
        throw LLUtil::HardwareIdError(os.str());
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;

    if (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
            &pclsObj, &uReturn);

        if(0 != uReturn)
        {
            VARIANT vtProp;
            hr = pclsObj->Get((LPCWSTR) fieldName, 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && V_VT(&vtProp) == VT_BSTR) {
                rezStr = vtProp.bstrVal;
                VariantClear(&vtProp);
            }
            pclsObj->Release();
        }
    }

    pEnumerator->Release();
}

void execQuery2(IWbemServices *pSvc, const BSTR fieldName, const BSTR objectName, bstr_t& rezStr)
{
    rezStr = _T("");

    bstr_t reqStr = bstr_t(_T("SELECT ")) + fieldName + _T(" FROM ") + objectName;

    HRESULT hres;

    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _T("WQL"), 
        reqStr,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);

    if (FAILED(hres)) {
        std::ostringstream os;
        os << "Query for operating system name failed. Error code = 0x"
            << " " << std::ios_base::hex << hres;
        throw LLUtil::HardwareIdError(os.str());
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    // Add all values to set to keep it sorted
    typedef std::set<CAtlString> StrSet;
    StrSet values;

    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;

    if (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        while(0 != uReturn)
        {
            VARIANT vtProp;
            hr = pclsObj->Get((LPCWSTR) fieldName, 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && V_VT(&vtProp) == VT_BSTR) {
                values.insert(CAtlString(vtProp).Trim());
                VariantClear(&vtProp);
            }
            pclsObj->Release();

            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        }
    }

    bstr_t t;
    
    pEnumerator->Release();

    for (StrSet::const_iterator ci = values.begin(); ci != values.end(); ++ci) {
        rezStr += bstr_t(ci->AllocSysString(), false);
    }
}

void getMacAddress(IWbemServices *pSvc, bstr_t& rezStr) {
    HRESULT hres;

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        _T("WQL"), 
        _T("SELECT * FROM Win32_NetworkAdapter WHERE PhysicalAdapter=true "),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        std::ostringstream os;
        os << "Query for Win32_NetworkAdapter failed. Error code = 0x"
            << " " << std::ios_base::hex << hres;
        throw LLUtil::HardwareIdError(os.str());
    }

    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;

    std::string classes[] = {"PCI", "USB"};
    std::vector<Device> devices;

    for (;;) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
            &pclsObj, &uReturn);

        if (0 == uReturn)
            break;

        Device device;

        VARIANT vtProp;
        hr = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            if (V_VT(&vtProp) == VT_BSTR) {
                std::string value = (const char*)(bstr_t)vtProp.bstrVal;
                VariantClear(&vtProp);

                for (const std::string& xclass : classes) {
                    if (value.substr(0, 3) == xclass) {
                        device.xclass = xclass;
                    }
                }

                if (device.xclass.empty())
                    continue;
            }
        } else {
            continue;
        }
        
        hr = pclsObj->Get(L"MACAddress", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            if (V_VT(&vtProp) == VT_BSTR) {
                device.mac = (const char *)(bstr_t)vtProp.bstrVal;
                VariantClear(&vtProp);
            }
        } else {
            continue;
        }

        if (!device.mac.empty())
            devices.push_back(device);

        pclsObj->Release();
    }

    pEnumerator->Release();

    std::sort(devices.begin(), devices.end(), [](const Device &device1, const Device &device2) {
        // Sort first by class, then by mac
        if (device1.xclass < device2.xclass)
            return true;
        else if (device1.xclass > device2.xclass)
            return false;
        else
            return device1.mac < device2.mac;
    });

    if (devices.empty())
        return;

    QString savedMac = QnGlobalSettings::instance()->savedMac();
    bool savedMacFound = std::find_if(devices.begin(), devices.end(), [savedMac](const Device& device) { return savedMac == device.mac.c_str(); }) != devices.end();
    if (!savedMacFound) {
        savedMac = devices.front().mac.c_str();
        QnGlobalSettings::instance()->setSavedMac(savedMac);
    }

    rezStr = savedMac.toLatin1();
}

unsigned short SwapShort(unsigned short a) {
  a = ((a & 0x00FF) << 8) | ((a & 0xFF00) >> 8);
  return a;
}

unsigned int SwapWord(unsigned int a) {
  a = ((a & 0x000000FF) << 24) |
      ((a & 0x0000FF00) <<  8) |
      ((a & 0x00FF0000) >>  8) |
      ((a & 0xFF000000) >> 24);
  return a;
}

void changeGuidByteOrder(bstr_t& guid) {
    if (guid.length() != 36)
        return;

    std::string guid_str((const char*)guid);
    LLUtil::changeGuidByteOrder(guid_str);
    guid = guid_str.c_str();
}

void calcHardwareId(QByteArray &hardwareId, IWbemServices *pSvc, int version, bool guidCompatibility)
{
    void (*execQuery)(IWbemServices *pSvc, const BSTR fieldName, const BSTR objectName, bstr_t& rezStr);

    execQuery = version == 1 ? execQuery1 : execQuery2;

    bstr_t boardID, boardUUID, boardManufacturer, boardProduct;
    bstr_t biosID, biosManufacturer;
    bstr_t hddID, hddManufacturer;
    bstr_t memoryPartNumber, memorySerialNumber;
    bstr_t mac;

    execQuery(pSvc, _T("UUID"), _T("Win32_ComputerSystemProduct"), boardUUID);

    if (!guidCompatibility)
        changeGuidByteOrder(boardUUID);

    execQuery(pSvc, _T("SerialNumber"), _T("Win32_BaseBoard"), boardID);
    execQuery(pSvc, _T("Manufacturer"), _T("Win32_BaseBoard"), boardManufacturer);
    execQuery(pSvc, _T("Product"), _T("Win32_BaseBoard"), boardProduct);

    execQuery(pSvc, _T("SerialNumber"), _T("CIM_BIOSElement"), biosID);
    execQuery(pSvc, _T("Manufacturer"), _T("CIM_BIOSElement"), biosManufacturer);

    execQuery(pSvc, _T("SerialNumber"), _T("Win32_PhysicalMedia"), hddID);
    execQuery(pSvc, _T("Manufacturer"), _T("Win32_PhysicalMedia"), hddManufacturer);
    execQuery(pSvc, _T("PartNumber"), _T("Win32_PhysicalMemory"), memoryPartNumber);
    execQuery(pSvc, _T("SerialNumber"), _T("Win32_PhysicalMemory"), memorySerialNumber);

    getMacAddress(pSvc, mac);

    if (boardID.length() || boardUUID.length() || biosID.length()) {
        hardwareId = (LPCSTR) (boardID + boardUUID + boardManufacturer + boardProduct + biosID + biosManufacturer);
        if (version >= 3) {
            hardwareId += (LPCSTR)(memoryPartNumber + memorySerialNumber);
        }

        if (version >= 4) {
            hardwareId += (LPCSTR)mac;
        }
    } else {
        hardwareId.clear();
    }
}

} // namespace {}

namespace LLUtil {
    void fillHardwareIds(QList<QByteArray>& hardwareIds);
}

void LLUtil::fillHardwareIds(QList<QByteArray>& hardwareIds)
{
    bool needUninitialize = true;
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
    if (FAILED(hres))
    {
        if (hres == RPC_E_CHANGED_MODE)
            needUninitialize = false;
        else {
            std::ostringstream os;
            os << "Failed to initialize COM library. Error code = 0x" 
                << std::ios_base::hex << hres;
            throw HardwareIdError(os.str());
        }
    }

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------
    // Note: If you are using Windows 2000, you need to specify -
    // the default authentication credentials for a user by using
    // a SOLE_AUTHENTICATION_LIST structure in the pAuthList ----
    // parameter of CoInitializeSecurity ------------------------

    hres =  CoInitializeSecurity(
        NULL, 
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities 
        NULL                         // Reserved
        );


    if (FAILED(hres))
    {
        std::ostringstream os;
        os << "Failed to initialize security. Error code = 0x" 
            << std::ios_base::hex << hres;
        if (needUninitialize) CoUninitialize();
        throw HardwareIdError(os.str());
    }

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------

    IWbemLocator *pLoc = NULL;

    hres = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &pLoc);

    if (FAILED(hres))
    {
        std::ostringstream os;
        os << "Failed to create IWbemLocator object."
            << " Err code = 0x"
            << std::ios_base::hex << hres;
        if (needUninitialize) CoUninitialize();
        throw HardwareIdError(os.str());
    }

    // Step 4: -----------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method

    IWbemServices *pSvc = NULL;

    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
        NULL,                    // User name. NULL = current user
        NULL,                    // User password. NULL = current
        0,                       // Locale. NULL indicates current
        0,                       // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object 
        &pSvc                    // pointer to IWbemServices proxy
        );

    if (FAILED(hres))
    {
        std::ostringstream os;
        os << "Could not connect. Error code = 0x" 
            << std::ios_base::hex << hres;
        pLoc->Release();     
        if (needUninitialize) CoUninitialize();
        throw HardwareIdError(os.str());
    }

    // qDebug() << "Connected to ROOT\\CIMV2 WMI namespace" << endl;


    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------

    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name 
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities 
        );

    if (FAILED(hres))
    {
        std::ostringstream os;
        os <<  "Could not set proxy blanket. Error code = 0x" 
            << std::ios_base::hex << hres;
        pSvc->Release();
        pLoc->Release();     
        if (needUninitialize) CoUninitialize();
        throw HardwareIdError(os.str());
    }

    calcHardwareId(hardwareIds[0], pSvc, 1, false);
    calcHardwareId(hardwareIds[1], pSvc, 2, false);
    calcHardwareId(hardwareIds[2], pSvc, 3, false);
    calcHardwareId(hardwareIds[3], pSvc, 4, false);
    calcHardwareId(hardwareIds[4], pSvc, 1, true);
    calcHardwareId(hardwareIds[5], pSvc, 2, true);
    calcHardwareId(hardwareIds[6], pSvc, 3, true);
    calcHardwareId(hardwareIds[7], pSvc, 4, true);

    // Cleanup
    // ========

    pSvc->Release();
    pLoc->Release();
    if (needUninitialize) CoUninitialize();
}
