#pragma once

#include <map>
#include <nx/utils/literal.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

static const std::map<QString, std::vector<QString>> kCarAttributeOptions = {
    {lit("Object Type"), {lit("Car")}},
    {lit("Vendor"), {
        lit("Nissan"),
        lit("Toyota"),
        lit("Mercedes"),
        lit("Audi"),
        lit("Citroen"),
        lit("VAZ"),
        lit("Fiat"),
        lit("Dodge"),
        lit("BMW"),
        lit("Great Wall")}}
};

static const std::map<QString, std::vector<QString>> kHumanAttributeOptions = {
    {lit("Object Type"), {lit("Human")}},
    {lit("Age"), {lit("Adult"), lit("Child")}}
};

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
