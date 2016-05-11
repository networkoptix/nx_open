#include "targetver.h"

#include <vector>
#include <iostream>
#include <array>

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

#include <nx/utils/log/log.h>

#include "util.h"
#include "licensing/hardware_info.h"
#include "hardware_id.h"
#include "hardware_id_p.h"

#define _WIN32_DCOM
# pragma comment(lib, "wbemuuid.lib")

namespace LLUtil {

    namespace {
        const int kWbemTimeoutMs = 5000;
    }

HRESULT GetDisabledNICS(IWbemServices* pSvc, std::vector<_bstr_t>& paths)
{
    HRESULT hres;

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery( L"WQL", L"SELECT * FROM Win32_NetworkAdapter WHERE PhysicalAdapter=true AND NetEnabled=false",
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

    if (FAILED(hres))
    {
        NX_LOG(QString(lit("GetDisabledNICS(): ExecQuery failed. Error code = 0x%1, Message: %2"))
            .arg((long)hres, 0, 16)
            .arg(QString::fromWCharArray(_com_error(hres).ErrorMessage()))
            , cl_logINFO);
        return 1;
    }

    // Get the data from the WQL sentence
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturnCount = 0;
    
    while (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturnCount);

        // Leave the loop is no items returned or request failed
        if(0 == uReturnCount || FAILED(hr))
            break;

        _bstr_t path;
        VARIANT vtProp;

        hr = pclsObj->Get(L"__Path", 0, &vtProp, 0, 0); // String
        if (!FAILED(hr))
        {
            if ((vtProp.vt != VT_NULL) && (vtProp.vt != VT_EMPTY) && !(vtProp.vt & VT_ARRAY))
                path = vtProp.bstrVal;

        }
        VariantClear(&vtProp);

        if (path.length() == 0)
            continue;

        paths.push_back(path);

        pclsObj->Release();
        pclsObj=NULL;
    }

    pEnumerator->Release();
    if (pclsObj!=NULL)
        pclsObj->Release();

    return S_OK;
}

HRESULT ExecuteMethodAtPaths(IWbemServices* pSvc, const TCHAR* methodName, const std::vector<_bstr_t>& paths)
{
    HRESULT hres  = S_OK;

    for (const _bstr_t& path: paths)
    {
        IWbemCallResult *result = NULL;
        hres = pSvc->ExecMethod(path, _bstr_t(methodName), 0, NULL, NULL, NULL, &result);
        if (hres != WBEM_S_NO_ERROR)
        {
            NX_LOG(QString(lit("ExecuteMethodAtPaths Method: %1 failed. Error code = 0x%2, Message: %3"))
                .arg(QString::fromWCharArray(methodName))
                .arg((long)hres, 0, 16)
                .arg(QString::fromWCharArray(_com_error(hres).ErrorMessage()))
                , cl_logINFO);

            continue;
        }

        LONG lStatus;
        result->GetCallStatus(kWbemTimeoutMs, &lStatus);
        if (lStatus != WBEM_S_NO_ERROR)
        {
            NX_LOG(QString(lit("ExecuteMethodAtPaths Method: %1 failed. GetCallStatus() failed. Error code = 0x%2, Message: %3"))
                .arg(QString::fromWCharArray(methodName))
                .arg((long)hres, 0, 16)
                .arg(QString::fromWCharArray(_com_error(hres).ErrorMessage()))
                , cl_logINFO);
        }
    }

    return hres;
}

HRESULT EnableNICSAtPaths(IWbemServices* pSvc, const std::vector<_bstr_t>& paths)
{
    return ExecuteMethodAtPaths(pSvc, _T("Enable"), paths);
}

HRESULT DisableNICSAtPaths(IWbemServices* pSvc, const std::vector<_bstr_t>& paths)
{
    return ExecuteMethodAtPaths(pSvc, _T("Disable"), paths);
}

static void findMacAddresses(IWbemServices* pSvc, QnMacAndDeviceClassList& devices)
{
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

    IWbemClassObject* pclsObj;
    ULONG uReturn = 0;

    QString classes[] = {"PCI", "USB"};

    for (;;)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
            &pclsObj, &uReturn);

        if (0 == uReturn)
            break;

        QnMacAndDeviceClass device;

        VARIANT vtProp;
        hr = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            if (V_VT(&vtProp) == VT_BSTR)
            {
                QString value = (const char*)(bstr_t)vtProp.bstrVal;
                VariantClear(&vtProp);

                for (const QString& xclass : classes)
                {
                    if (value.mid(0, 3) == xclass)
                    {
                        device.xclass = xclass;
                    }
                }

                if (device.xclass.isEmpty())
                    continue;
            }
        } else
        {
            continue;
        }

        hr = pclsObj->Get(L"MACAddress", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            if (V_VT(&vtProp) == VT_BSTR)
            {
                device.mac = (const char *)(bstr_t)vtProp.bstrVal;
                VariantClear(&vtProp);
            }
        } else
        {
            continue;
        }

        if (!device.mac.isEmpty())
            devices.push_back(device);

        pclsObj->Release();
    }

    pEnumerator->Release();
}

static QString execQueryForHWID1(IWbemServices* pSvc, const BSTR fieldName, const BSTR objectName)
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
            if (SUCCEEDED(hr) && V_VT(&vtProp) == VT_BSTR)
            {
                rezStr = vtProp.bstrVal;
                VariantClear(&vtProp);
            }
            pclsObj->Release();
        }
    }

    pEnumerator->Release();

    return (LPCSTR)rezStr;
}

static QString execQuery2(IWbemServices* pSvc, const BSTR fieldName, const BSTR objectName)
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
            if (SUCCEEDED(hr) && V_VT(&vtProp) == VT_BSTR)
            {
                values.insert(CAtlString(vtProp).Trim());
                VariantClear(&vtProp);
            }
            pclsObj->Release();

            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        }
    }

    bstr_t t;

    pEnumerator->Release();

    for (StrSet::const_iterator ci = values.begin(); ci != values.end(); ++ci)
    {
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

typedef QString (*ExecQueryFunction)(IWbemServices* pSvc, const BSTR fieldName, const BSTR objectName);

static void fillHardwareInfo(IWbemServices* pSvc, ExecQueryFunction execQuery, QnHardwareInfo& hardwareInfo)
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

static QMap<QString, QString> calcHardwareIds(const QnHardwareInfo& hi, int version, bool guidCompatibility)
{
    QMap<QString, QString> result;

    QString hardwareId;

    if (hi.boardID.length() || hi.boardUUID.length() || hi.biosID.length())
    {
        hardwareId = hi.boardID + (guidCompatibility ? hi.compatibilityBoardUUID : hi.boardUUID) + hi.boardManufacturer + hi.boardProduct + hi.biosID + hi.biosManufacturer;
        if (version == 3)
        {
            hardwareId += hi.memoryPartNumber + hi.memorySerialNumber;
        }
    }

    if (version == 4 || version == 5)
    {
        for (const auto& nic : hi.nics)
        {
            const QString& mac = nic.mac;

            if (!mac.isEmpty())
            {
                result[mac] = hardwareId + mac;
            }
        }
    } else
    {
        result[""] = hardwareId;
    }

    return result;
}

void calcHardwareIds(QMap<QString, QStringList>& macHardwareIds, const QnHardwareInfo& hardwareInfo, int version) {
    macHardwareIds.clear();

    QMap<QString, QString> hardwareIdMap;

    std::array<bool, 2> guidCompatibilities = { false, true };

    for (bool guidCompatibility : guidCompatibilities) {
        hardwareIdMap = calcHardwareIds(hardwareInfo, version, guidCompatibility);
        for (QString mac : hardwareIdMap.keys()) {
            macHardwareIds[mac] << hardwareIdMap[mac];
        }
    }

    for (const QString& mac : macHardwareIds.keys()) {
        macHardwareIds[mac].removeDuplicates();
    }
}

} // namespace {}

namespace LLUtil {
    void fillHardwareIds(HardwareIdListType& hardwareIds, QnHardwareInfo& hardwareInfo);
}

void LLUtil::fillHardwareIds(HardwareIdListType& hardwareIds, QnHardwareInfo& hardwareInfo)
{
    bool needUninitialize = true;
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres =  CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        if (hres == RPC_E_CHANGED_MODE)
        {
            needUninitialize = false;
        }
        else
        {
            std::ostringstream os;
            os << "Failed to initialize COM library. Error code = 0x"
                << std::ios_base::hex << hres;
            throw HardwareIdError(os.str());
        }
    }

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------

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

    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    bool vistaOrLater = false;

    if (GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi)))
    {
        vistaOrLater = osvi.dwMajorVersion >= 6;
    }

    // Find disabled NICs
    std::vector<_bstr_t> paths;

    if (vistaOrLater)
    {
        if (GetDisabledNICS(pSvc, paths) == S_OK)
        {
            // Temporarily enable them
            if (EnableNICSAtPaths(pSvc, paths) == S_OK)
            {
                // Wait up to 10 seconds for all interfaces to be enabled
                for (int i = 0; i < 10; i++)
                {
                    std::vector<_bstr_t> tmpPaths;
                    GetDisabledNICS(pSvc, tmpPaths);

                    if (!tmpPaths.empty())
                    {
                        break;
                    } else
                    {
                        Sleep(1000);
                    }
                }
            }
        }
    }

    findMacAddresses(pSvc, hardwareInfo.nics);

    if (vistaOrLater)
    {
        // Disable enabled NICs again if were any
        DisableNICSAtPaths(pSvc, paths);
    }

    QMap<QString, QStringList> macHardwareIds;

    // Only for HWID1
    QnHardwareInfo v1HardwareInfo;
    fillHardwareInfo(pSvc, execQueryForHWID1, v1HardwareInfo);

    calcHardwareIds(macHardwareIds, v1HardwareInfo, 1);
    hardwareIds << macHardwareIds;

    // For HWID>1
    fillHardwareInfo(pSvc, execQuery2, hardwareInfo);
    for (int i = 2; i <= LATEST_HWID_VERSION; i++)
    {
        calcHardwareIds(macHardwareIds, hardwareInfo, i);
        hardwareIds << macHardwareIds;
    }

    // Cleanup
    // ========

    pSvc->Release();
    pLoc->Release();
    if (needUninitialize) CoUninitialize();
}
