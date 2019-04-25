#include "maxmind_db.h"

#include <nx/utils/log/log.h>

namespace nx::maxmind {

namespace {

struct Mmdb: public MMDB_s
{
    ~Mmdb()
    {
        MMDB_close(this);
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------
// MaxmindDb

bool MaxmindDb::open(const std::string& dbPath)
{
    if (m_db)
        m_db.reset();

    auto db = std::make_unique<Mmdb>();

    int result = MMDB_open(dbPath.c_str(), MMDB_MODE_MMAP, db.get());
    if (result != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Failed to open maxmind db file: %1, error: %2", MMDB_strerror(result));
        return false;
    }

    m_db = std::move(db);

    return true;
}

} // namespace nx::maxmind