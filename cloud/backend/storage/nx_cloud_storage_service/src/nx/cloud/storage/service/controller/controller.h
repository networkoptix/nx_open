#pragma once

namespace nx::cloud::storage::service {

class Settings;

class Controller
{
public:
    Controller(const Settings& settings);

private:
    //const Settings& m_settings;
};

} // namespace nx::cloud::storage::service