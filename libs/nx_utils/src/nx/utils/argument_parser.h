// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QString>

namespace nx {

/**
 * Stand-alone & simple function for parsing arguments.
 * Supports both "-a value" and "--arg=value" syntax. "-" and "--" are omitted.
 */
class NX_UTILS_API ArgumentParser
{
public:
    ArgumentParser(int argc = 0, const char* argv[] = nullptr);
    ArgumentParser(const QStringList& args);
    ArgumentParser(ArgumentParser&&) = default;
    ArgumentParser(const ArgumentParser&) = default;

    void parse(int argc, const char* argv[]);
    void parse(const QStringList& args);

    bool read(const QString& name, QString* const value) const;
    bool read(const QString& name, std::string* const value) const;
    bool read(const QString& name, int* const value) const;
    bool read(const QString& name, size_t* const value) const;
    bool read(const QString& name, double* const value) const;

    template<typename ValueType = QString, typename MainName, typename ... AltNames>
    std::optional<ValueType> get(MainName mainName, AltNames ... altNames) const;

    template<typename ValueType = QString>
    std::optional<ValueType> get() const { return std::nullopt; }

    bool contains(const QString& name) const;

    template<typename Handler>
    void forEach(const QString& name, const Handler& handler) const;

    std::multimap<QString, QString> allArgs() const;

    /**
     * @return Arguments without name in the order of appearance.
     */
    std::vector<QString> getPositionalArgs() const;

private:
    template<typename ValueType = QString>
    std::optional<ValueType> getImpl(const QString& name) const;

    template<typename ValueType = QString>
    std::optional<ValueType> getImpl(const char* name) const;

    std::multimap<QString, QString> m_args;
    std::vector<QString> m_positionalArgs;
};

template<typename ValueType, typename MainName, typename ... AltNames>
std::optional<ValueType> ArgumentParser::get(MainName mainName, AltNames ... altNames) const
{
    if (const auto value = getImpl<ValueType>(std::forward<MainName>(mainName)))
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

template<typename ValueType>
std::optional<ValueType> ArgumentParser::getImpl(const QString& name) const
{
    ValueType value;
    if (read(name, &value))
        return value;
    else
        return std::nullopt;
}

template<typename ValueType>
std::optional<ValueType> ArgumentParser::getImpl(const char* name) const
{
    return get<ValueType>(QLatin1String(name));
}

} // namespace nx
