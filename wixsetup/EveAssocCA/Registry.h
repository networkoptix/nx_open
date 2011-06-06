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
    RegistryManager();

    void backupAssociation(LPCWSTR ext, LPCWSTR association);

private:
    CRegKey m_regKey;
};

#endif // _EVEASSOC_CA_REGISTRY_H_