#include "basicServerSettingsIo.h"

#include <nx/utils/log/log.h>

#include <charconv>

using namespace std::literals;

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

namespace basicServerSettingsIo
{

void deserializeOrThrow(const char* source, bool* destination)
{
    if (!source)
        throw ServerValueError();

    if (strcmp(source, "true") == 0)
    {
        *destination = true;
        return;
    }
    if (strcmp(source, "false") == 0)
    {
        *destination = false;
        return;
    }

    throw ServerValueError();
}
std::string serialize(bool value)
{
    if (value)
        return "true"s;
    else
        return "false"s;

}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, int* destination)
{
    if (!source)
        throw ServerValueError();

    const char* end = source + strlen(source);
    std::from_chars_result conversionResult = std::from_chars(source, end, *destination);
    if (conversionResult.ptr == end)
        return;

    throw ServerValueError();
}

std::string serialize(int value)
{
    return std::to_string(value);
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, std::string* destination)
{
    if (!source)
        throw ServerValueError();

    *destination = source;
}

std::string serialize(std::string value)
{
    return value;
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, UnnamedBoxFigure* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<UnnamedBoxFigure> tmp = UnnamedBoxFigure::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(const UnnamedBoxFigure& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, ObjectSizeConstraints* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<ObjectSizeConstraints> tmp = ObjectSizeConstraints::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(const ObjectSizeConstraints& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, NamedPolygon* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<NamedPolygon> tmp = NamedPolygon::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(const NamedPolygon& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, UnnamedPolygon* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<UnnamedPolygon> tmp = UnnamedPolygon::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(const UnnamedPolygon& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, NamedLineFigure* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<NamedLineFigure> tmp = NamedLineFigure::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(const NamedLineFigure& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, Width* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<Width> tmp = ServerStringToWidth(source);
    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(std::pair<Width, Height> pair)
{
    std::optional<std::vector<PluginPoint>> points = WidthHeightToPluginPoints(
        pair.first, pair.second);
    std::string result = pluginPointsToServerString(*points);
    return result;
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, std::vector<PluginPoint>* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<std::vector<PluginPoint>> tmp = ServerStringToPluginPoints(source);
    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serialize(const std::vector<PluginPoint>& /*value*/)
{
    return {};
}

}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
