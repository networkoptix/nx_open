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

    SynchronizationEngine& synchronizationEngine();

private:
    SynchronizationEngine m_synchronizationEngine;
};

} // namespace nx::clusterdb::engine
