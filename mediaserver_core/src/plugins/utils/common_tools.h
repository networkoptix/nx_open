#pragma once

#include <vector>

#include <QtCore/QSize>

namespace nx {
namespace mediaserver_core {
namespace plugins {

static const QSize kDefaultSecondaryResolution(480, 316);

QSize secondaryStreamResolution(
    const QSize& primaryStreamResolution,
    const std::vector<QSize>& resolutionList);

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
