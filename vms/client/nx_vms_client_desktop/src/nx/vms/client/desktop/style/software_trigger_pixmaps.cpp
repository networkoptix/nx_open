// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_pixmaps.h"

#include <set>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>

#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/skin.h>

namespace nx::vms::client::desktop {

namespace {

enum IconType { Solid, Outline };

static const QString kPixmapExtension = ".svg";

QString pixmapPath(IconType type)
{
    return (type == Solid)
        ? "24x24/Solid/"
        : "24x24/Outline/";
}

static const std::map<QString, std::tuple<IconType, QString>> kIconNameMapping = {
    {"_alarm_off",              {Solid,     "st_alarm_off"}},
    {"_alarm_on",               {Solid,     "st_alarm_on"}},
    {"_bell_off",               {Solid,     "st_bell_off"}},
    {"_bell_on",                {Solid,     "st_bell_on"}},
    {"_door_closed",            {Solid,     "st_door_closed"}},
    {"_door_opened",            {Solid,     "st_door_opened"}},
    {"_elevator_doors_close",   {Solid,     "st_elevator_doors_close"}},
    {"_elevator_doors_open",    {Solid,     "st_elevator_doors_open"}},
    {"_elevator_down",          {Solid,     "st_elevator_down"}},
    {"_elevator_up",            {Solid,     "st_elevator_up"}},
    {"_escalator__start",       {Solid,     "st_escalator__start"}},
    {"_escalator__stop",        {Solid,     "st_escalator__stop"}},
    {"_escalator_down",         {Solid,     "st_escalator_down"}},
    {"_escalator_up",           {Solid,     "st_escalator_up"}},
    {"_hvac_fan_start",         {Solid,     "st_hvac_fan_start"}},
    {"_hvac_fan_stop",          {Solid,     "st_hvac_fan_stop"}},
    {"_hvac_temp_down",         {Solid,     "st_hvac_temp_down"}},
    {"_hvac_temp_up",           {Solid,     "st_hvac_temp_up"}},
    {"_lights2_off",            {Solid,     "st_lights2_off"}},
    {"_lights2_on",             {Solid,     "st_lights2_on"}},
    {"_lights_off",             {Solid,     "st_lights_off"}},
    {"_lights_on",              {Solid,     "st_lights_on"}},
    {"_lock_locked",            {Solid,     "st_lock_locked"}},
    {"_lock_unlocked",          {Solid,     "st_lock_unlocked"}},
    {"_parking_gates_closed",   {Solid,     "st_parking_gates_closed"}},
    {"_parking_gates_open",     {Solid,     "st_parking_gates_open"}},
    {"_power_off",              {Solid,     "st_power_off"}},
    {"_power_on",               {Solid,     "st_power_on"}},
    {"_simple_arrow_h_left",    {Outline,   "arrow_left_2px"}},
    {"_simple_arrow_h_right",   {Outline,   "arrow_right_2px"}},
    {"_simple_arrow_v_down",    {Outline,   "arrow_down_2px"}},
    {"_simple_arrow_v_up",      {Outline,   "arrow_up_2px"}},
    {"_simple_minus",           {Solid,     "st_simple_minus"}},
    {"_simple_plus",            {Solid,     "st_simple_plus"}},
    {"_thumb_down",             {Solid,     "st_thumb_down"}},
    {"_thumb_up",               {Solid,     "st_thumb_up"}},
    {"_wipers_on",              {Solid,     "st_wipers_on"}},
    {"_wipers_wash",            {Solid,     "st_wipers_wash"}},
    {"ban",                     {Solid,     "st_ban"}},
    {"bookmark",                {Solid,     "st_bookmark"}},
    {"bubble",                  {Solid,     "st_bubble"}},
    {"circle",                  {Solid,     "st_circle"}},
    {"fire",                    {Solid,     "st_fire"}},
    {"hang_up",                 {Solid,     "deny"}},
    {"heater",                  {Solid,     "st_heater"}},
    {"message",                 {Solid,     "stmessage"}},
    {"mic",                     {Solid,     "st_mic"}},
    {"mute_call",               {Solid,     "st_microphone_off"}},
    {"object_tracking_stop",    {Outline,   "close"}},
    {"puzzle",                  {Solid,     "st_puzzle"}},
    {"siren",                   {Solid,     "st_siren"}},
    {"speaker",                 {Solid,     "st_speaker"}},
    {"unmute_call",             {Solid,     "st_microphone"}},
    {"wiper",                   {Solid,     "st_wiper"}},
};

QPixmap getTriggerPixmap(const QString& name)
{
    if (name.isEmpty())
        return QPixmap();

    return qnSkin->pixmap(SoftwareTriggerPixmaps::effectivePixmapPath(name), true);
}

} // namespace

QString SoftwareTriggerPixmaps::defaultPixmapName()
{
    return "_bell_on";
}

QString SoftwareTriggerPixmaps::effectivePixmapName(const QString& name)
{
    return hasPixmap(name) ? name : defaultPixmapName();
}

QString SoftwareTriggerPixmaps::effectivePixmapPath(const QString& name)
{
    if (const auto it = kIconNameMapping.find(name);
        it != kIconNameMapping.end())
    {
        const auto [type, fileName] = it->second;
        return pixmapPath(type) + fileName + kPixmapExtension;
    }

    return effectivePixmapPath(defaultPixmapName());
}

const QStringList& SoftwareTriggerPixmaps::pixmapNames()
{
    static const auto pixmapNames =
        []() -> QStringList
        {
            std::set<QString, decltype(&nx::utils::naturalStringLess)> names(
                &nx::utils::naturalStringLess);

            for (const auto& [key, value]: kIconNameMapping)
                names.insert(key);

            QStringList result;
            for (const auto& name: names)
                result.emplace_back(name);

            return result;
        }();

    return pixmapNames;
}

const SoftwareTriggerPixmaps::MapT SoftwareTriggerPixmaps::pixmaps()
{
    static const auto pixmaps =
        []() -> MapT
        {
            MapT result;
            for (auto& name: pixmapNames())
                result.push_back({name, getTriggerPixmap(name)});

            return result;
        }();

    return pixmaps;
}

bool SoftwareTriggerPixmaps::hasPixmap(const QString& name)
{
    return !getTriggerPixmap(name).isNull();
}

QPixmap SoftwareTriggerPixmaps::pixmapByName(const QString& name)
{
    const auto pixmap = getTriggerPixmap(name);
    return pixmap.isNull()
        ? getTriggerPixmap(defaultPixmapName())
        : pixmap;
}

} // namespace nx::vms::client::desktop
