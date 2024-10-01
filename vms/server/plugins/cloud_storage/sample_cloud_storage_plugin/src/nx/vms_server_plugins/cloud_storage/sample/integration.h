// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/cloud_storage/i_integration.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/result.h>

namespace nx::vms_server_plugins::cloud_storage::sample {

class Integration: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IIntegration>
{
public:
    virtual ~Integration() override;
    virtual void setUtilityProvider(nx::sdk::IUtilityProvider* utilityProvider) override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;

private:
    virtual void doObtainEngine(
        const char* url,
        const nx::sdk::cloud_storage::IArchiveUpdateHandler* archiveUpdateHandler,
        nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>* outResult) override;
};

} // nx::vms_server_plugins::cloud_storage::sample
