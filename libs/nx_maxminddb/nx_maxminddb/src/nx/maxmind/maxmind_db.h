#pragma once

#include <string>
#include <memory>
#include <optional>

#include <QtCore/QString>

#include <maxminddb.h>

namespace nx::maxmind {

namespace detail {

template<typename ...ConstCharPtrs>
std::pair<int /*MMDB status*/, std::string> getStringValue(
    MMDB_lookup_result_s& lookupResult,
    ConstCharPtrs... strings)
{
    if (!lookupResult.found_entry)
        return {0, {}};

    MMDB_entry_data_s entryData;
    int result = MMDB_get_value(&lookupResult.entry, &entryData, strings..., nullptr);

    if (result != MMDB_SUCCESS)
        return {result, {}};

    if (entryData.type != MMDB_DATA_TYPE_UTF8_STRING)
        return {result, {}};

    return std::make_pair(
        result,
        QString::fromUtf8(entryData.utf8_string, entryData.data_size).toStdString());
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// MaxmindDb

class NX_MAXMINDDB_API MaxmindDb
{
public:
    bool open(const std::string& dbPath);

    std::string lookupCountry(const std::string& ipAddress, const std::string& language = "en");
    std::string lookupContinent(const std::string& ipAddress, const std::string& language = "en");

private:
    std::optional<MMDB_lookup_result_s> lookupIpAddress(const std::string& ipAddress);

private:
    std::unique_ptr<MMDB_s> m_db;
};

} // namespace nx::maxind