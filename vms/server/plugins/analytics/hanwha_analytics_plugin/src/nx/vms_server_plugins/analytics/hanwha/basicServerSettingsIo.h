#pragma once

#include "point.h"

#include <string>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------
struct ServerValueError {};

namespace basicServerSettingsIo
{

void deserializeOrThrow(const char* source, bool* destination);
std::string serialize(bool value);

void deserializeOrThrow(const char* source, int* destination);
std::string serialize(int value);

void deserializeOrThrow(const char* source, std::string* destination);
std::string serialize(std::string value);

void deserializeOrThrow(const char* source, Direction* destination);
std::string serialize(Direction value);

void deserializeOrThrow(const char* source, UnnamedBoxFigure* destination);
std::string serialize(const UnnamedBoxFigure& value);

void deserializeOrThrow(const char* source, UnnamedPolygon* destination);
std::string serialize(const UnnamedPolygon& value);

void deserializeOrThrow(const char* source, NamedPolygon* destination);
std::string serialize(const NamedPolygon& value);

void deserializeOrThrow(const char* source, NamedLineFigure* destination);
std::string serialize(const NamedLineFigure& value);

void deserializeOrThrow(const char* source, Width* destination);
std::string serialize(Width value);

void deserializeOrThrow(const char* source, std::vector<PluginPoint>* destination);
std::string serialize(const std::vector<PluginPoint>& value);

}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
