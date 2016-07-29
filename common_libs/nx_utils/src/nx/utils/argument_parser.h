#pragma once

#include <boost/optional.hpp>
#include <map>
#include <list>
#include <functional>
#include <QString>


namespace nx {
namespace utils {

//!Stand-alone & simple function for parsing arguments
/*!
    Supports both "-a value" and "--arg=value" syntax. "-" and "--" are ommitted
*/
class NX_UTILS_API ArgumentParser
{
public:
    ArgumentParser(int argc = 0, char **argv = nullptr);
    void parse(int argc, char **argv);

    bool read(const QString& name, QString* const value) const;
    bool read(const QString& name, int* const value) const;
    bool read(const QString& name, size_t* const value) const;

    template<typename ValueType = QString>
    boost::optional<ValueType> get(const QString& name) const;

    template<typename ValueType = QString>
    boost::optional<ValueType> get(const char* name) const;

    template<typename ValueType = QString, typename MainName, typename ... AltNames>
    boost::optional<ValueType> get(MainName mainName, AltNames ... altNames) const;

    template<typename ValueType = QString>
    boost::optional<ValueType> get() const { return boost::none; }

    template<typename Handler>
    void forEach(const QString& name, const Handler& handler) const;

private:
    std::multimap<QString, QString> m_args;
};

// ---

template<typename ValueType>
boost::optional<ValueType> ArgumentParser::get(const QString& name) const
{
    ValueType value;
    if (read(name, &value))
        return value;
    else
        return boost::none;
}

template<typename ValueType>
boost::optional<ValueType> ArgumentParser::get(const char* name) const
{
    return get<ValueType>(QLatin1String(name));
}

template<typename ValueType, typename MainName, typename ... AltNames>
boost::optional<ValueType> ArgumentParser::get(MainName mainName, AltNames ... altNames) const
{
    if (const auto value = get<ValueType>(std::forward<MainName>(mainName)))
        return value;

    return get<ValueType>(std::forward<AltNames>(altNames) ...);
}

template<typename Handler>
void ArgumentParser::forEach(const QString& name, const Handler& handler) const
{
    const auto range = m_args.equal_range(name);
    for (auto it = range.first; it != range.second; it++)
        handler(it->second);
}

} // namespace utils
} // namespace nx
