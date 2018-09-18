#pragma once

#include <string>

#include "../synchronization_engine.h"

namespace nx::data_sync_engine {

class Settings;
class Model;

class Controller
{
public:
    Controller(
        const std::string& applicationId,
        const Settings& settings,
        Model* model);

    SyncronizationEngine& syncronizationEngine();

private:
    SyncronizationEngine m_syncronizationEngine;
};

} // namespace nx::data_sync_engine
