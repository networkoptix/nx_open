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
#ifdef _WIN32
    auto path = QString(dbPath.c_str()).toUtf8();
#else
    auto path = dbPath.c_str();
#endif

    auto db = std::make_unique<Mmdb>();

    int result = MMDB_open(path, MMDB_MODE_MMAP, db.get());
    if (result != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Failed to open maxmind db file: %1, error: %2", 
            path, MMDB_strerror(result));
        return false;
    }

    m_db = std::move(db);

    return true;
}

std::string MaxmindDb::lookupCountry(const std::string& ipAddress, const std::string& language)
{
    auto lookupResult = lookupIpAddress(ipAddress);
    if (!lookupResult)
        return {};

    int mmdbError = MMDB_SUCCESS;
    std::string value;
    std::tie(mmdbError, value) =
        detail::getStringValue(*lookupResult, "country", "names", language.c_str());

    if (mmdbError != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Error from MMDB_get_value: %1", MMDB_strerror(mmdbError));
        return {};
    }

    return value;
}

std::string MaxmindDb::lookupContinent(const std::string& ipAddress, const std::string& language)
{
    auto lookupResult = lookupIpAddress(ipAddress);
    if (!lookupResult)
        return {};

    int mmdbError = MMDB_SUCCESS;
    std::string value;
    std::tie(mmdbError, value) =
        detail::getStringValue(*lookupResult, "continent", "names", language.c_str());

    if (mmdbError != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Error from MMDB_get_value: %1", MMDB_strerror(mmdbError));
        return {};
    }

    return value;
}

std::optional<MMDB_lookup_result_s> MaxmindDb::lookupIpAddress(const std::string& ipAddress)
{
    if (!m_db)
        return std::nullopt;

    int gaiError = 0;
    int mmdbError = MMDB_SUCCESS;
    auto lookupResult = MMDB_lookup_string(m_db.get(), ipAddress.c_str(), &gaiError, &mmdbError);

    if (gaiError != 0)
    {
        NX_VERBOSE(this, "Error from getaddrinfo: %1", gai_strerror(gaiError));
        return std::nullopt;
    }

    if (mmdbError != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Error from MMDB_lookup_string: %1", MMDB_strerror(mmdbError));
        return std::nullopt;
    }

    return lookupResult;
}

} // namespace nx::maxmind