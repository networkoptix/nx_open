#pragma once

#include <string>
#include <memory>
#include <optional>

#include <QtCore/QString>
#include <maxminddb.h>

#include <nx/utils/log/log.h>

#include "utils.h"

namespace nx::maxmind {

struct Geopoint
{
    double latitude = 0;
    double longitude = 0;

    std::string toString() const
    {
        return "{ lat: " + std::to_string(latitude) + ", lon: " + std::to_string(longitude) + " }";
    }
};

//-------------------------------------------------------------------------------------------------
// MaxmindDb

class NX_MAXMINDDB_API MaxmindDb
{
public:
    bool open(const std::string& dbPath);

    std::optional<std::string> lookupCountry(
        const std::string& ipAddress,
        const std::string& language = "en");
    std::optional<std::string> lookupContinent(
        const std::string& ipAddress,
        const std::string& language = "en");
    std::optional<Geopoint> lookupGeopoint(const std::string& ipAddress);

private:
    std::optional<MMDB_lookup_result_s> lookupIpAddress(const std::string& ipAddress);

    template<typename ...ConstCharPtrs>
    std::optional<std::string> getString(
        const char * callingFunc,
        MMDB_lookup_result_s& lookupResult,
        ConstCharPtrs... strings);

    template<typename ...ConstCharPtrs>
    std::optional<double> getDouble(
        const char* callingFunc, 
        MMDB_lookup_result_s& lookupResult,
        ConstCharPtrs... strings);
    
    bool validate(
        const char* callingFunc,
        int mmdbError,
        const MMDB_entry_data_s& entryData,
        uint32_t expectedMmdbDataType) const;

private:
    std::unique_ptr<MMDB_s> m_db;
};

//-------------------------------------------------------------------------------------------------

template<typename ...ConstCharPtrs>
std::optional<std::string> MaxmindDb::getString(
    const char * callingFunc,
    MMDB_lookup_result_s& lookupResult,
    ConstCharPtrs... strings)
{
    MMDB_entry_data_s entryData;
    int error = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);
    if (!validate(callingFunc, error, entryData, MMDB_DATA_TYPE_UTF8_STRING))
        return std::nullopt;

    return QString::fromUtf8(entryData.utf8_string, entryData.data_size).toStdString();
}

template<typename ...ConstCharPtrs>
std::optional<double> MaxmindDb::getDouble(
    const char* callingFunc, 
    MMDB_lookup_result_s& lookupResult,
    ConstCharPtrs... strings)
{
    MMDB_entry_data_s entryData;
    int error = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);
    if (!validate(callingFunc, error, entryData, MMDB_DATA_TYPE_DOUBLE))
        return std::nullopt;

    return entryData.double_value;
}

} // namespace nx::maxind