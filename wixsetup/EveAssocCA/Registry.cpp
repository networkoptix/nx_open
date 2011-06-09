#include "stdafx.h"

#include "Registry.h"

RegistryManager::RegistryManager()
{
    CAtlString regKey(L"Software\\Network Optix\\EVE media player\\FileAssociationBackup\\");

    if (m_regKey.Open(HKEY_CURRENT_USER, regKey, KEY_WRITE) != ERROR_SUCCESS)
    {
        if (m_regKey.Create(HKEY_CURRENT_USER, regKey, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, NULL) != S_OK)
        {
            throw RegistryError("Can't create registry key");
        }
    }
}

void RegistryManager::backupAssociation(LPCWSTR ext, LPCWSTR association)
{
    m_regKey.SetStringValue(ext, association);
}
