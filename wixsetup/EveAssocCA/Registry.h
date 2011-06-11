#ifndef _EVEASSOC_CA_REGISTRY_H_
#define _EVEASSOC_CA_REGISTRY_H_

#include "Errors.h"

class RegistryError : public Error
{
public:
    RegistryError(const char* description)
        : Error(description)
    {
    }
};

class RegistryManager
{
public:
    typedef CSimpleArray<CAtlString> StringList;

    RegistryManager();

    void setBackupAssociation(LPCWSTR ext, LPCWSTR association);
    bool isExtSupported(LPCWSTR ext);
    CAtlString getBackupAssociation(LPCWSTR ext);

private:
    CRegKey m_regKey;
    CRegKey m_suppRegKey;
};

class ProgIdDictionary
{
public:
    ProgIdDictionary();

    CAtlString appNameFromProgId(const LPCTSTR progId);
private:
    typedef CSimpleArray<CAtlString> StringList;

    bool listRegistryKey(const LPCTSTR key, StringList& values);
    CAtlString getRegValue(CRegKey& regKey, const LPCTSTR name);

    CAtlMap<CAtlString, CAtlString> m_progIdToAppMap;

    static const int MAX_REGISTRY_VALUE_SIZE = 1024;
};

#endif // _EVEASSOC_CA_REGISTRY_H_