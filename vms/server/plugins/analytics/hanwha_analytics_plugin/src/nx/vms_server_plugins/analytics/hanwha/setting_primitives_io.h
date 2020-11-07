#pragma once

#include "setting_primitives.h"

#include <string>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
namespace SettingPrimitivesServerIo
{

struct DeserializationError {};

void deserializeOrThrow(const char* source, bool* destination);
std::string serialize(bool value);

void deserializeOrThrow(const char* source, int* destination);
std::string serialize(int value);

void deserializeOrThrow(const char* source, std::string* destination);
std::string serialize(std::string value);

void deserializeOrThrow(const char* source, ObjectSizeConstraints* destination);
std::string serialize(const ObjectSizeConstraints& value);

void deserializeOrThrow(const char* source, UnnamedPolygon* destination);
std::string serialize(const UnnamedPolygon& value);

void deserializeOrThrow(const char* source, NamedPolygon* destination);
std::string serialize(const NamedPolygon& value);

void deserializeOrThrow(const char* source, NamedLineFigure* destination);
std::string serialize(const NamedLineFigure& value);

} // namespace SettingPrimitivesServerIo

//-------------------------------------------------------------------------------------------------

namespace SettingPrimitivesDeviceIo
{

struct CameraResponseJsonError {};

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution /*roiResolution*/, bool* result);
std::string serialize(bool value);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution /*roiResolution*/, int* result);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution /*roiResolution*/, std::string* result);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution roiResolution, PluginPoint* result);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution roiResolution,
    std::vector<PluginPoint>* result);
std::string serialize(const std::vector<PluginPoint>& points, RoiResolution roiResolution);

void deserializeOrThrow(
    const nx::kit::Json& json, RoiResolution roiResolution, ObjectSizeConstraints* result);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution roiResolution, UnnamedPolygon* result);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution /*roiResolution*/, Direction* result);

void deserializeOrThrow(
    const nx::kit::Json& json, const char* key, RoiResolution /*roiResolution*/,
    bool* result, const char* desired);

} // namespace SettingPrimitivesDeviceIo

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
