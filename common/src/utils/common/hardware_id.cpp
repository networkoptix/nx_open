#ifdef Q_OS_WIN

#define _WIN32_DCOM
#include <iostream>
using namespace std;
#include <comdef.h>
#include <Wbemidl.h>
# pragma comment(lib, "wbemuuid.lib")
#include "qcryptographichash.h"

#include "hardware_id.h"
#include <QByteArray>

int execQuery(IWbemServices *pSvc, const char* fieldName, const char* objectName, QString& rezStr)
{
    QString reqStr(QLatin1String("SELECT %1 FROM %2"));

    HRESULT hres;

    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"), 
        //bstr_t("SELECT * FROM Win32_BaseBoard"),
        //bstr_t("SELECT SerialNumber FROM CIM_BIOSElement"),
        bstr_t(reqStr.arg(QLatin1String(fieldName)).arg(QLatin1String(objectName)).toUtf8().data()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << hex << hres << endl;
        return 1;               // Program has failed.
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
            // Get the value of the Name property
            //hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
            hr = pclsObj->Get((LPCWSTR) QString(QLatin1String(fieldName)).data(), 0, &vtProp, 0, 0);
            rezStr = QString::fromUtf16(vtProp.bstrVal).trimmed();
            VariantClear(&vtProp);
            pclsObj->Release();
        }
    }

    pEnumerator->Release();

    return 0;
}

QString getHardwareId()
{
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
    if (FAILED(hres))
    {
        cout << "Failed to initialize COM library. Error code = 0x" 
            << hex << hres << endl;
        return QString();                  // Program has failed.
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
        cout << "Failed to initialize security. Error code = 0x" 
            << hex << hres << endl;
        CoUninitialize();
        return QString();                    // Program has failed.
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
        cout << "Failed to create IWbemLocator object."
            << " Err code = 0x"
            << hex << hres << endl;
        CoUninitialize();
        return QString();                 // Program has failed.
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
        NULL,                    // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object 
        &pSvc                    // pointer to IWbemServices proxy
        );

    if (FAILED(hres))
    {
        cout << "Could not connect. Error code = 0x" 
            << hex << hres << endl;
        pLoc->Release();     
        CoUninitialize();
        return QString();                // Program has failed.
    }

    cout << "Connected to ROOT\\CIMV2 WMI namespace" << endl;


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
        cout << "Could not set proxy blanket. Error code = 0x" 
            << hex << hres << endl;
        pSvc->Release();
        pLoc->Release();     
        CoUninitialize();
        return QString();               // Program has failed.
    }

    QString boardID, boardManufacturer, boardProduct;
    QString biosID, biosManufacturer;
    QString hddID, hddManufacturer;

    hres = execQuery(pSvc, "SerialNumber", "Win32_BaseBoard", boardID);
    hres = execQuery(pSvc, "Manufacturer", "Win32_BaseBoard", boardManufacturer);
    hres = execQuery(pSvc, "Product", "Win32_BaseBoard", boardProduct);

    hres = execQuery(pSvc, "SerialNumber", "CIM_BIOSElement", biosID);
    hres = execQuery(pSvc, "Manufacturer", "CIM_BIOSElement", biosManufacturer);

    hres = execQuery(pSvc, "SerialNumber", "Win32_PhysicalMedia", hddID);
    hres = execQuery(pSvc, "Manufacturer", "Win32_PhysicalMedia", hddManufacturer);

    // Cleanup
    // ========

    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    QByteArray result;
    if (!boardID.isEmpty() || !biosID.isEmpty())
        result = (boardID + boardManufacturer + boardProduct + biosID + biosManufacturer).toUtf8();
    else if (!hddID.isEmpty())
        result = (hddID + hddManufacturer).toUtf8();
    else
        return QString();
    
    QByteArray hash = QCryptographicHash::hash(result, QCryptographicHash::Md5);
    return QString::fromLocal8Bit(hash.toHex());
}

#endif
