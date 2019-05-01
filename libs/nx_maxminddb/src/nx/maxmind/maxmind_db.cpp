#include "maxmind_db.h"

#include <nx/utils/log/log.h>

namespace nx::maxmind {

namespace {

ResultCode toResultCode(int mmdbError)
{
    switch (mmdbError)
    {
        case MMDB_SUCCESS:
            return ResultCode::ok;
        case MMDB_INVALID_LOOKUP_PATH_ERROR:
        case MMDB_FILE_OPEN_ERROR:
        case MMDB_CORRUPT_SEARCH_TREE_ERROR:
        case MMDB_INVALID_METADATA_ERROR:
        case MMDB_IO_ERROR:
        case MMDB_OUT_OF_MEMORY_ERROR:
        case MMDB_UNKNOWN_DATABASE_FORMAT_ERROR:
            return ResultCode::ioError;
        default:
            return ResultCode::unknownError;
    }
}

std::optional<Continent> toContinent(const std::string& continent)
{
    if (continent == "Africa")
        return Continent::africa;
    if (continent == "Antarctica")
        return Continent::antarctica;
    if (continent == "Asia")
        return Continent::asia;
    if (continent == "Australia")
        return Continent::australia;
    if (continent == "Europe")
        return Continent::europe;
    if (continent == "North America")
        return Continent::northAmerica;
    if (continent == "South America")
        return Continent::southAmerica;
    return std::nullopt;
}

const char* dataTypeToString(int mmdbDataType)
{
    switch (mmdbDataType)
    {
        case MMDB_DATA_TYPE_EXTENDED:
            return "extended";
        case MMDB_DATA_TYPE_POINTER:
            return "pointer";
        case MMDB_DATA_TYPE_UTF8_STRING:
            return "utf8 string";
        case MMDB_DATA_TYPE_DOUBLE:
            return "double";
        case MMDB_DATA_TYPE_BYTES:
            return "bytes";
        case MMDB_DATA_TYPE_UINT16:
            return "uint16";
        case MMDB_DATA_TYPE_UINT32:
            return "uint32";
        case MMDB_DATA_TYPE_MAP:
            return "map";
        case MMDB_DATA_TYPE_INT32:
            return "int32";
        case MMDB_DATA_TYPE_UINT64:
            return "uint64";
        case MMDB_DATA_TYPE_UINT128:
            return "uint128";
        case MMDB_DATA_TYPE_ARRAY:
            return "array";
        case MMDB_DATA_TYPE_CONTAINER:
            return "container";
        case MMDB_DATA_TYPE_END_MARKER:
            return "end marker";
        case MMDB_DATA_TYPE_BOOLEAN:
            return "boolean";
        case MMDB_DATA_TYPE_FLOAT:
            return "float";
        default:
            return "unknown";
    }
}

struct Mmdb: public MMDB_s
{
    ~Mmdb()
    {
        MMDB_close(this);
    }
};

static constexpr char kContinent[] = "continent";
static constexpr char kCountry[] = "country";
static constexpr char kNames[] = "names";
static constexpr char kLanguage[] = "en";
static constexpr char kLocation[] = "location";
static constexpr char kLatitude[] = "latitude";
static constexpr char kLongitude[] = "longitude";

} // namespace

//-------------------------------------------------------------------------------------------------
// MaxmindDb

bool MaxmindDb::open(const std::string& dbPath)
{
    auto db = std::make_unique<Mmdb>();

    int result = MMDB_open(dbPath.c_str(), MMDB_MODE_MMAP, db.get());
    if (result != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Failed to open maxmind db file: %1, error: %2",
            dbPath, MMDB_strerror(result));
        false;
    }

    m_db = std::move(db);

    return true;
}

std::pair<ResultCode, Location> MaxmindDb::lookup(const std::string& ipAddress)
{
    ResultCode resultCode;
    MMDB_lookup_result_s lookupResult;
    std::tie(resultCode, lookupResult) = lookupIpAddress(ipAddress);
    if (resultCode != ResultCode::ok)
        return {resultCode, {}};

    Location location;
    std::string str;
    std::tie(resultCode, str) = getString(lookupResult, kContinent, kNames, kLanguage);
    if (resultCode != ResultCode::ok)
        return {resultCode, Location{}};
    auto continent = toContinent(str);
    if (!continent)
        return {ResultCode::notFound, Location{}};
    location.continent = *continent;

    // Not every ip has an associated country, for example: "2.16.126.1", which contains
    // continent and location (geopoint).
    // if not found, it should not be an error
    std::tie(resultCode, location.country) = getString(lookupResult, kCountry, kNames, kLanguage);

    // Geopoint exists only in Geolite2-City mmdb, so if it is not found, it should not be
    // an error.
    Geopoint geopoint;
    std::tie(resultCode, geopoint) = getGeopoint(lookupResult);
    if (resultCode == ResultCode::ok)
        location.geopoint = std::move(geopoint);

    return {resultCode, location};
}

std::pair<ResultCode, MMDB_lookup_result_s> MaxmindDb::lookupIpAddress(const std::string& ipAddress)
{
    if (!m_db)
        return {ResultCode::ioError, {}};

    int gaiError = 0;
    int mmdbError = MMDB_SUCCESS;
    auto lookupResult = MMDB_lookup_string(m_db.get(), ipAddress.c_str(), &gaiError, &mmdbError);

    if (gaiError != 0)
    {
        NX_VERBOSE(this, "Error from getaddrinfo: %1", gai_strerror(gaiError));
        return {ResultCode::ioError, {}};
    }

    if (mmdbError != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Error from MMDB_lookup_string: %1", MMDB_strerror(mmdbError));
        return {toResultCode(mmdbError), {}};
    }

    if (!lookupResult.found_entry)
        return {ResultCode::notFound, {}};

    return {ResultCode::ok, lookupResult};
}

std::pair<ResultCode, Geopoint> MaxmindDb::getGeopoint(MMDB_lookup_result_s& lookupResult)
{
    ResultCode resultCode;
    Geopoint geopoint;

    std::tie(resultCode, geopoint.latitude) = getDouble(lookupResult, kLocation, kLatitude);
    if (resultCode != ResultCode::ok)
        return {resultCode, geopoint};

    std::tie(resultCode, geopoint.longitude)= getDouble(lookupResult, kLocation, kLongitude);
    if (resultCode != ResultCode::ok)
        return {resultCode, geopoint};

    return {resultCode, geopoint};
}

ResultCode MaxmindDb::validate(
    int mmdbError,
    const MMDB_entry_data_s& entryData,
    uint32_t expectedMmdbDataType) const
{
    if (mmdbError != MMDB_SUCCESS)
    {
        NX_VERBOSE(this, "Error from MMDB_get_value: %1", MMDB_strerror(mmdbError));
        return toResultCode(mmdbError);
    }

    if (!entryData.has_data)
        return ResultCode::notFound;

    if (entryData.type != expectedMmdbDataType)
    {
        NX_VERBOSE(this, "Type mismatch error, expected mmdb data type: %1, got %2",
            dataTypeToString(expectedMmdbDataType),
            dataTypeToString(entryData.type));
        return ResultCode::unknownError;
    }

    return ResultCode::ok;
}

} // namespace nx::maxmind