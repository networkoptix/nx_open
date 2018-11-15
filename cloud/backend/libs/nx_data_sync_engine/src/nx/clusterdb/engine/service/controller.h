#pragma once

#include <string>

#include "../synchronization_engine.h"

namespace nx::clusterdb::engine {

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

} // namespace nx::clusterdb::engine
