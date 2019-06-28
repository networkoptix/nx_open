#include "resolver.h"

#include <maxminddb.h>

namespace nx::geo_ip {

namespace {

static constexpr char kContinent[] = "continent";
static constexpr char kCode[] = "code";
static constexpr char kCountry[] = "country";
static constexpr char kNames[] = "names";
static constexpr char kLanguage[] = "en";
static constexpr char kLocation[] = "location";
static constexpr char kLatitude[] = "latitude";
static constexpr char kLongitude[] = "longitude";
static constexpr char kSubdivisions[] = "subdivisions";
static constexpr char kCity[] = "city";

bool isNormal(ResultCode resultCode)
{
    return resultCode == ResultCode::ok || resultCode == ResultCode::notFound;
}

ResultCode toResultCode(int mmdbError)
{
    switch (mmdbError)
    {
        case MMDB_SUCCESS:
            return ResultCode::ok;
        case MMDB_FILE_OPEN_ERROR:
        case MMDB_IO_ERROR:
        case MMDB_OUT_OF_MEMORY_ERROR:
            return ResultCode::ioError;
        case MMDB_INVALID_LOOKUP_PATH_ERROR:
        case MMDB_LOOKUP_PATH_DOES_NOT_MATCH_DATA_ERROR:
            return ResultCode::notFound;
        default:
            return ResultCode::unknownError;
    }
}

std::optional<Continent> toContinent(const std::string& continent)
{
    if (continent == "AF" || continent == "Africa")
        return Continent::africa;
    if (continent == "AN" || continent == "Antarctica")
        return Continent::antarctica;
    if (continent == "AS" || continent == "Asia")
        return Continent::asia;
    // using Australia as continent but maxmind calls it Oceania and calls the country Australia
    if (continent == "OC" || continent == "Oceania" || continent == "Australia")
        return Continent::australia;
    if (continent == "EU" || continent == "Europe")
        return Continent::europe;
    if (continent == "NA" || continent == "North America")
        return Continent::northAmerica;
    if (continent == "SA" ||continent == "South America")
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

std::string gaiStringError(int gaiError)
{
#if _WIN32 && UNICODE
    std::wstring wstr = gai_strerror(gaiError);
    return std::string(wstr.begin(), wstr.end());
#else
    return gai_strerror(gaiError);
#endif
}

} // namespace

//-------------------------------------------------------------------------------------------------
// ResolverImpl

class Resolver::ResolverImpl
{
public:
    /**
     * throws exception if database file fails to open
     */
    ResolverImpl(const std::string& mmdbPath)
    {
        int result = MMDB_open(mmdbPath.c_str(), MMDB_MODE_MMAP, &m_db);
        if (result != MMDB_SUCCESS)
            throw Exception(toResultCode(result), MMDB_strerror(result));
    }

    ~ResolverImpl()
    {
        MMDB_close(&m_db);
    }

    Location resolve(const std::string& ipAddress)
    {
        auto lookupResult = lookupIpAddress(ipAddress);

        std::string continentString;
        auto result = getString(&continentString, lookupResult, kContinent, kCode);
        if (result.code != ResultCode::ok)
            throw Exception(result.code, result.error);

        auto continent = toContinent(continentString);
        if (!continent)
        {
            throw Exception(
                ResultCode::unknownError,
                "Failed to resolve string: " + continentString + " to a continent");
        }

        Location location(*continent);

        Country country;
        result = getCountry(&country, lookupResult);
        if (!isNormal(result.code))
            throw Exception(result.code, result.error);

        Geopoint geopoint;
        result = getGeopoint(&geopoint, lookupResult);
        if (!isNormal(result.code))
            throw Exception(result.code, result.error);

        if (result.code == ResultCode::ok)
            location.country = std::move(country);

        result = getString(&location.city, lookupResult, kCity, kNames, kLanguage);
        if (!isNormal(result.code))
            throw Exception(result.code, result.error);

        if (result.code == ResultCode::ok)
            location.geopoint = std::move(geopoint);

        return location;
    }

private:
    struct Result
    {
        ResultCode code;
        std::string error;
    };

    MMDB_lookup_result_s lookupIpAddress(const std::string& ipAddress)
    {
        int gaiError = 0;
        int mmdbError = MMDB_SUCCESS;
        auto lookupResult =
            MMDB_lookup_string(&m_db, ipAddress.c_str(), &gaiError, &mmdbError);

        if (gaiError != 0)
        {
            throw Exception(
                ResultCode::unknownError,
                std::string("getaddrinfo: ") + gaiStringError(gaiError));
        }

        if (mmdbError != MMDB_SUCCESS)
        {
            throw Exception(
                toResultCode(mmdbError),
                std::string("MMDB_lookup_string: ") + MMDB_strerror(mmdbError));
        }

        if (!lookupResult.found_entry)
            throw Exception(ResultCode::notFound, ipAddress + " not found in database file");

        return lookupResult;
    }

    Result validate(
        int mmdbError,
        const MMDB_entry_data_s& entryData,
        uint32_t expectedMmdbDataType) const
    {
        if (mmdbError != MMDB_SUCCESS)
            return {toResultCode(mmdbError), MMDB_strerror(mmdbError)};

        if (!entryData.has_data)
            return {ResultCode::notFound, "MMDB_entry_data_s structure is empty"};

        if (entryData.type != expectedMmdbDataType)
        {
            return {
                ResultCode::unknownError,
                std::string("Type mismatch, expected: ") + dataTypeToString(expectedMmdbDataType)
                    + ", got: " + dataTypeToString(entryData.type)};
        }

        return {ResultCode::ok, {}};
    }

    template<typename ...ConstCharPtrs>
    Result getString(
        std::string* outString,
        MMDB_lookup_result_s& lookupResult,
        ConstCharPtrs... strings)
    {
        outString->clear();
        MMDB_entry_data_s entryData;
        int mmdbResult = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);
        auto result = validate(mmdbResult, entryData, MMDB_DATA_TYPE_UTF8_STRING);
        if (result.code == ResultCode::ok)
            *outString = std::string(entryData.utf8_string, entryData.data_size);
        return result;
    }

    /** Looks up a value along lookup path: "subdivisions", std::to_string(index), strings...; */
    template<typename ...ConstCharPtrs>
    Result getSubdivision(
        std::string* outSubdivision,
        MMDB_lookup_result_s& lookupResult,
        int index,
        ConstCharPtrs... strings)
    {
        auto result = getString(
            outSubdivision,
            lookupResult,
            kSubdivisions, std::to_string(index).c_str(), strings...);
        return result;
    }

    template<typename ...ConstCharPtrs>
    Result getDouble(double* outDouble, MMDB_lookup_result_s& lookupResult, ConstCharPtrs... strings)
    {
        *outDouble = std::numeric_limits<double>::min();
        MMDB_entry_data_s entryData;
        int mmdbResult = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);
        auto result = validate(mmdbResult, entryData, MMDB_DATA_TYPE_DOUBLE);
        if (result.code == ResultCode::ok)
            *outDouble = entryData.double_value;
        return result;
    }

    Result getCountry(Country* outCountry, MMDB_lookup_result_s& lookupResult)
    {
        auto result = getString(&outCountry->name, lookupResult, kCountry, kNames, kLanguage);
        if (!isNormal(result.code))
            throw Exception(result.code, result.error);

        result = getSubdivision(&outCountry->subdivision, lookupResult, 0, kNames, kLanguage);
        if (!isNormal(result.code))
            throw Exception(result.code, result.error);

        return result;
    }

    Result getGeopoint(Geopoint* outGeopoint, MMDB_lookup_result_s& lookupResult)
    {
        auto result = getDouble(&outGeopoint->latitude, lookupResult, kLocation, kLatitude);
        if (result.code != ResultCode::ok)
            return result;
        return getDouble(&outGeopoint->longitude, lookupResult, kLocation, kLongitude);
    }

private:
    MMDB_s m_db;
};

//-------------------------------------------------------------------------------------------------
// Resolver

Resolver::Resolver(const std::string& mmdbPath):
	m_impl(std::make_unique<ResolverImpl>(mmdbPath))
{
}

Resolver::~Resolver() = default;

Location Resolver::resolve(const std::string& ipAddress)
{
    return m_impl->resolve(ipAddress);
}

} // namespace nx::geo_ip