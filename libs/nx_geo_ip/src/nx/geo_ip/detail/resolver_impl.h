#pragma once

#include <string>
#include <memory>

#include <maxminddb.h>

#include "nx/geo_ip/types.h"

namespace nx::geo_ip{

class Settings;

namespace detail {

class ResolverImpl
{
public:
    ResolverImpl(const Settings& settings);
    virtual ~ResolverImpl() = default;

    ResultCode initialize();

    Result resolve(const std::string& ipAddress);

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
        if (resultCode != ResultCode::ok)
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
        if (resultCode != ResultCode::ok)
            return {resultCode, 0};

        return {resultCode, entryData.double_value};
    }

private:
    const Settings& m_settings;
    std::unique_ptr<MMDB_s> m_db;
};

} // namespace detail
} // namespace nx::geo_ip
