#pragma once

#ifndef Q_MOC_RUN
#include <boost/preprocessor/stringize.hpp>
#endif

#include <QtCore/QByteArray>

namespace nx::vms::client::core::graphics {

QByteArray modernizeShaderSource(const QByteArray& source);

QByteArray processShaderSource(const char* parenthesized);

} // namespace nx::vms::client::core::graphics

#define QN_SHADER_SOURCE(...) \
    nx::vms::client::core::graphics::processShaderSource(BOOST_PP_STRINGIZE((__VA_ARGS__)))
