#include "stdafx.h"

#include "Registry.h"

RegistryManager::RegistryManager()
{
    CAtlString regKey(L"Software\\Network Optix\\EVE media player\\FileAssociationBackup\\");

    if (m_regKey.Open(HKEY_CURRENT_USER, regKey, KEY_WRITE | KEY_READ) != ERROR_SUCCESS)
    {
        if (m_regKey.Create(HKEY_CURRENT_USER, regKey, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, NULL, NULL) != ERROR_SUCCESS)
        {
            throw RegistryError("Can't create registry key 'Software\\Network Optix\\EVE media player\\FileAssociationBackup'");
        }
    }

    if (m_suppRegKey.Open(HKEY_CLASSES_ROOT, L"Applications\\EvePlayer-Beta.exe\\SupportedTypes", KEY_READ) != ERROR_SUCCESS)
    {
        throw RegistryError("Can't open registry key 'Applications\\EvePlayer-Beta.exe\\SupportedTypes'");
    }
}

void RegistryManager::setBackupAssociation(LPCWSTR ext, LPCWSTR association)
{
    m_regKey.SetStringValue(ext, association);
}

CAtlString RegistryManager::getBackupAssociation(LPCWSTR ext)
{
    TCHAR pszValue[1024];
    DWORD pnValueLength = 1023;

    pszValue[0] = 0;
    m_regKey.QueryStringValue(ext, pszValue, &pnValueLength);

    return pszValue;
}

bool RegistryManager::isExtSupported(LPCWSTR ext)
{
    TCHAR pszValue[1];
    DWORD pnValueLength = 0;

    return m_suppRegKey.QueryStringValue(ext, pszValue, &pnValueLength) == ERROR_MORE_DATA;
}

CAtlString ProgIdDictionary::appNameFromProgId(const LPCTSTR name)
{
    CAtlMap<CAtlString, CAtlString>::CPair* pair = m_progIdToAppMap.Lookup(name);

    if (pair)
        return pair->m_value;
    else
        return L"";
}

bool ProgIdDictionary::listRegistryKey(const LPCTSTR key, StringList& values)
{
    CRegKey regAppsKey;
    if (regAppsKey.Open(HKEY_LOCAL_MACHINE, key, KEY_READ) != ERROR_SUCCESS)
        return false;

    DWORD index = 0;
    TCHAR value[MAX_REGISTRY_VALUE_SIZE];
    DWORD valueSize = MAX_REGISTRY_VALUE_SIZE;

    while (RegEnumValue(regAppsKey, index, value, &valueSize, 0, 0, 0, 0) == ERROR_SUCCESS)
    {
        if (wcslen(value) == 0)
        {
            valueSize = 1024;
            index++;

            continue;
        }

        values.Add(value);

        valueSize = 1024;
        index++;
    }

    return true;
}

CAtlString ProgIdDictionary::getRegValue(CRegKey& regKey, const LPCTSTR name)
{
    TCHAR value[MAX_REGISTRY_VALUE_SIZE];
    DWORD valueSize = MAX_REGISTRY_VALUE_SIZE;
    regKey.QueryStringValue(name, value, &valueSize);

    return CAtlString(value);
}

ProgIdDictionary::ProgIdDictionary()
{
    StringList regApps;
    listRegistryKey(L"SOFTWARE\\RegisteredApplications", regApps);

    CRegKey regKey;
    regKey.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\RegisteredApplications", KEY_READ);

    int regAppsSize = regApps.GetSize();
    for (int i = 0; i < regAppsSize; i++)
    {
        CAtlString regApp = regApps[i];

        CAtlString fileAssociationPath = getRegValue(regKey, regApp) + L"\\FileAssociations";

        StringList fileExts;
        listRegistryKey(fileAssociationPath, fileExts);

        CRegKey capaKey;
        capaKey.Open(HKEY_LOCAL_MACHINE, fileAssociationPath);

        int fileExtsSize = fileExts.GetSize();
        for (int j = 0; j < fileExtsSize; j++)
        {
            CAtlString progId = getRegValue(capaKey, fileExts[j]);
            m_progIdToAppMap.SetAt(progId, regApp);
        }
    }
}
