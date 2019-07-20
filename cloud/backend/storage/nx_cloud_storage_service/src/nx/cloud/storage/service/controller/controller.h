#pragma once

#include <memory>

namespace nx::cloud::storage::service {

class Settings;
class AbstractStorageManager;

class Controller
{
public:
    Controller(const Settings& settings);
    ~Controller();

    AbstractStorageManager* storageManager();

private:
    //const Settings& m_settings;
    std::unique_ptr<AbstractStorageManager> m_storageManager;
};

} // namespace nx::cloud::storage::service