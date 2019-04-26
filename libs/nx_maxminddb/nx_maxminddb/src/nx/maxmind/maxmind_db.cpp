#include "maxmind_db.h"

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

std::optional<std::string> MaxmindDb::lookupCountry(
    const std::string& ipAddress,
    const std::string& language)
{
    auto lookupResult = lookupIpAddress(ipAddress);
    if (!lookupResult)
        return {};

    return getString(__func__, *lookupResult, "country", "names", language.c_str());
}

std::optional<std::string> MaxmindDb::lookupContinent(
    const std::string& ipAddress, 
    const std::string& language)
{
    auto lookupResult = lookupIpAddress(ipAddress);
    if (!lookupResult)
        return std::nullopt;

    return getString(__func__, *lookupResult, "continent", "names", language.c_str());
}

std::optional<Geopoint> MaxmindDb::lookupGeopoint(const std::string& ipAddress)
{
    auto lookupResult = lookupIpAddress(ipAddress);
    if (!lookupResult)
        return {};

    auto lat = getDouble(__func__, *lookupResult, "location", "latitude");
    if (!lat)
        return std::nullopt;

    auto lon = getDouble(__func__, *lookupResult, "location", "longitude");
    if (!lon)
        return std::nullopt;

    return Geopoint{*lat, *lon};
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

    if (!lookupResult.found_entry)
        return std::nullopt;

    return lookupResult;
}

bool MaxmindDb::validate(
    const char* callingFunc,
    int mmdbError,
    const MMDB_entry_data_s& entryData,
    uint32_t expectedMmdbDataType) const
{
    if (mmdbError != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "%1: error from MMDB_get_value: %2", 
            callingFunc, MMDB_strerror(mmdbError));
        return false;
    }
    
    if (!entryData.has_data)
        return false;

    if (entryData.type != expectedMmdbDataType)
    {
        NX_VERBOSE(this, "%1: expected mmdb data type: %2, got %3",
            callingFunc,
            utils::dataTypeToString(expectedMmdbDataType),
            utils::dataTypeToString(entryData.type));
        return false;
    }

    return true;
}

} // namespace nx::maxmind