#pragma once

#include <string>
#include <memory>
#include <optional>

#include <maxminddb.h>

#include <nx/utils/log/log.h>

namespace nx::maxmind {

enum class ResultCode
{
    Ok,
    NotFound,
    IoError,
    UnknownError
};

enum class Continent
{
    Unknown,
    Africa,
    Antarctica,
    Asia,
    Australia,
    Europe,
    NorthAmerica,
    SouthAmerica
};

struct Geopoint
{
    double latitude = 0;
    double longitude = 0;

    std::string toString() const
    {
        return "{ lat: " + std::to_string(latitude) + ", lon: " + std::to_string(longitude) + " }";
    }
};

struct Location
{
    Continent continent;
    std::string country;
    std::optional<Geopoint> geopoint;
};

//-------------------------------------------------------------------------------------------------
// MaxmindDb

class NX_MAXMINDDB_API MaxmindDb
{
public:
    bool open(const std::string& dbPath);

    std::pair<ResultCode, Location> lookup(const std::string& ipAddress);

private:
    std::pair<ResultCode, MMDB_lookup_result_s> lookupIpAddress(const std::string& ipAddress);

    std::pair<ResultCode, Geopoint> getGeopoint(MMDB_lookup_result_s& lookupResult);

    ResultCode validate(
        int mmdbError,
        const MMDB_entry_data_s& entryData,
        uint32_t expectedMmdbDataType) const;

    template<typename ...ConstCharPtrs>
    std::pair<ResultCode, std::string> getString(
        MMDB_lookup_result_s& lookupResult,
        ConstCharPtrs... strings)
    {
        MMDB_entry_data_s entryData;
        int mmdbResult = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);
        ResultCode resultCode = validate(mmdbResult, entryData, MMDB_DATA_TYPE_UTF8_STRING);
        if (resultCode != ResultCode::Ok)
            return {resultCode, {}};

        return {resultCode, std::string(entryData.utf8_string, entryData.data_size)};
    }

    template<typename ...ConstCharPtrs>
    std::pair<ResultCode, double> getDouble(
        MMDB_lookup_result_s& lookupResult,
        ConstCharPtrs... strings)
    {
        MMDB_entry_data_s entryData;
        int mmdbResult = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);
        ResultCode resultCode = validate(mmdbResult, entryData, MMDB_DATA_TYPE_DOUBLE);
        if (resultCode != ResultCode::Ok)
            return {resultCode, 0};

        return {resultCode, entryData.double_value};
    }

private:
    std::unique_ptr<MMDB_s> m_db;
};

} // namespace nx::maxind