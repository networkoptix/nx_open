#pragma once
#include "third_party_storage.h"
#include "common.h"

// {2E2C7A3D-256D-4018-B40E-512D72510BEC}
static const nxpl::NX_GUID IID_StorageFactory =
{ { 0x2e, 0x2c, 0x7a, 0x3d, 0x25, 0x6d, 0x40, 0x18, 0xb4, 0xe, 0x51, 0x2d, 0x72, 0x51, 0xb, 0xec } };

class TestStorageFactory
    : public nx_spl::StorageFactory,
      public PluginRefCounter<TestStorageFactory>
{
public:
    virtual const char** STORAGE_METHOD_CALL findAvailable() const override;

    virtual nx_spl::Storage* STORAGE_METHOD_CALL createStorage(
        const char* url,
        int*        ecode
    ) override;

    virtual const char* STORAGE_METHOD_CALL storageType() const override;

    virtual const char* lastErrorMessage(int ecode) const override;

public: // plugin interface implementation
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;
};
