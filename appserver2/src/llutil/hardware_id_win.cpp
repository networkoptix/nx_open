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
#include <QtCore/QSettings>

#include "util.h"
#include "hardware_id.h"
#include "hardware_id_pvt.h"

#define _WIN32_DCOM
# pragma comment(lib, "wbemuuid.lib")

namespace LLUtil {

static void findMacAddresses(IWbemServices *pSvc, DevicesList& devices) {
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
        os << "Query for operating system name failed. Error code = 0x"
            << " " << std::ios_base::hex << hres;
        throw LLUtil::HardwareIdError(os.str());
    }

    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;

    QString classes[] = {"PCI", "USB"};

    for (;;) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
            &pclsObj, &uReturn);

        if (0 == uReturn)
            break;

        DeviceClassAndMac device;

        VARIANT vtProp;
        hr = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            if (V_VT(&vtProp) == VT_BSTR) {
                QString value = (const char*)(bstr_t)vtProp.bstrVal;
                VariantClear(&vtProp);

                for (const QString& xclass : classes) {
                    if (value.mid(0, 3) == xclass) {
                        device.xclass = xclass;
                    }
                }

                if (device.xclass.isEmpty())
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

        if (!device.mac.isEmpty())
            devices.push_back(device);

        pclsObj->Release();
    }

    pEnumerator->Release();
}



static QByteArray getMacAddress(const DevicesList &devices,  QSettings *settings) {
    if (devices.empty())
        return QByteArray();

    return getSaveMacAddress(devices, settings).toUtf8();
}

static QString execQueryForHWID1(IWbemServices *pSvc, const BSTR fieldName, const BSTR objectName)
{
    bstr_t rezStr = _T("");

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

    return (LPCSTR)rezStr;
}

static QString execQuery2(IWbemServices *pSvc, const BSTR fieldName, const BSTR objectName)
{
    bstr_t rezStr = _T("");

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

    return (LPCSTR)rezStr;
}

static unsigned short SwapShort(unsigned short a)
{
  a = ((a & 0x00FF) << 8) | ((a & 0xFF00) >> 8);
  return a;
}

static unsigned int SwapWord(unsigned int a)
{
  a = ((a & 0x000000FF) << 24) |
      ((a & 0x0000FF00) <<  8) |
      ((a & 0x00FF0000) >>  8) |
      ((a & 0xFF000000) >> 24);
  return a;
}

typedef QString (*ExecQueryFunction)(IWbemServices *pSvc, const BSTR fieldName, const BSTR objectName);

static void fillHardwareInfo(IWbemServices *pSvc, ExecQueryFunction execQuery, HardwareInfo& hardwareInfo)
{
    hardwareInfo.compatibilityBoardUUID = execQuery(pSvc, _T("UUID"), _T("Win32_ComputerSystemProduct"));
    hardwareInfo.boardUUID = changedGuidByteOrder(hardwareInfo.compatibilityBoardUUID);

    hardwareInfo.boardID = execQuery(pSvc, _T("SerialNumber"), _T("Win32_BaseBoard"));
    hardwareInfo.boardManufacturer = execQuery(pSvc, _T("Manufacturer"), _T("Win32_BaseBoard"));
    hardwareInfo.boardProduct = execQuery(pSvc, _T("Product"), _T("Win32_BaseBoard"));

    hardwareInfo.biosID = execQuery(pSvc, _T("SerialNumber"), _T("CIM_BIOSElement"));
    hardwareInfo.biosManufacturer = execQuery(pSvc, _T("Manufacturer"), _T("CIM_BIOSElement"));

    hardwareInfo.memoryPartNumber = execQuery(pSvc, _T("PartNumber"), _T("Win32_PhysicalMemory"));
    hardwareInfo.memorySerialNumber = execQuery(pSvc, _T("SerialNumber"), _T("Win32_PhysicalMemory"));
}

static void calcHardwareId(QString &hardwareId, const HardwareInfo& hi, int version, bool guidCompatibility)
{
    if (hi.boardID.length() || hi.boardUUID.length() || hi.biosID.length()) {
        hardwareId = hi.boardID + (guidCompatibility ? hi.compatibilityBoardUUID : hi.boardUUID) + hi.boardManufacturer + hi.boardProduct + hi.biosID + hi.biosManufacturer;
        if (version == 3) {
            hardwareId += hi.memoryPartNumber + hi.memorySerialNumber;
        }
    } else {
        hardwareId.clear();
    }

    if (version == 4 && hi.mac.length() > 0)
        hardwareId += hi.mac;
}

} // namespace {}

namespace LLUtil {
    void fillHardwareIds(QStringList& hardwareIds, QSettings *settings, HardwareInfo& hardwareInfo);
}

void LLUtil::fillHardwareIds(QStringList& hardwareIds, QSettings *settings, HardwareInfo& hardwareInfo)
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

    // NOTE: on the client CoInitializeSecurity might be already executed,
    //       which should not leed to the error
    if (FAILED(hres) && hres != RPC_E_TOO_LATE)
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


    findMacAddresses(pSvc, hardwareInfo.nics);
    hardwareInfo.mac = getMacAddress(hardwareInfo.nics, settings);

    // Only for HWID1
    HardwareInfo v1HardwareInfo;
    fillHardwareInfo(pSvc, execQueryForHWID1, v1HardwareInfo);
    calcHardwareId(hardwareIds[0], v1HardwareInfo, 1, false);
    calcHardwareId(hardwareIds[LATEST_HWID_VERSION], v1HardwareInfo, 1, true);

    // For HWID>1
    fillHardwareInfo(pSvc, execQuery2, hardwareInfo);
    for (int i = 1; i < LATEST_HWID_VERSION; i++) {
        calcHardwareId(hardwareIds[i], hardwareInfo, i + 1, false);
        calcHardwareId(hardwareIds[LATEST_HWID_VERSION + i], hardwareInfo, i + 1, true);
    }

    // Cleanup
    // ========

    pSvc->Release();
    pLoc->Release();
    if (needUninitialize) CoUninitialize();
}
