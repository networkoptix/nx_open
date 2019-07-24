#pragma once

#include <memory>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }

namespace model {

class Database;

class Model
{
public:
    Model(const conf::Settings& settings);
    ~Model();

    void stop();

    Database* database();

private:
    std::unique_ptr<Database> m_database;
};

} // namespace model
} // namespace nx::cloud::storage::service
