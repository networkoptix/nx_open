#pragma once

#include <string>
#include <memory>

#include <maxminddb.h>

namespace nx::maxmind {

class NX_MAXMINDDB_API MaxmindDb
{
public:
    bool open(const std::string& dbPath);

private:
    std::unique_ptr<MMDB_s> m_db;
};

} // namespace nx::maxind