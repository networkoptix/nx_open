// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <mutex>

#include <nx/sdk/cloud_storage/i_integration.h>
#include <nx/sdk/helpers/ref_countable.h>

#include "data_manager.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

class Integration: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IIntegration>
{
public:
    virtual ~Integration() override;
    virtual void setUtilityProvider(nx::sdk::IUtilityProvider* utilityProvider) override;

protected:
    virtual void doObtainEngine(
        const char* url,
        const nx::sdk::cloud_storage::IArchiveUpdateHandler* archiveUpdateHandler,
        nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>* outResult) override;

    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;

private:
    nx::sdk::Ptr<nx::sdk::IUtilityProvider> m_utilityProvider;
    nx::sdk::Ptr<nx::sdk::cloud_storage::IEngine> m_engine;
    std::mutex m_mutex;
    std::shared_ptr<DataManager> m_dataManager;
};

} // nx::vms_server_plugins::cloud_storage::stub
