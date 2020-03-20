#pragma once

#include <tuple>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/stringize.hpp>
#endif

#include <QtCore/QObject>
#include <QtCore/QByteArray>

namespace nx::vms::client::core::graphics {

class ShaderHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    using base_type::base_type; //< Forward constructors.

    // Prepend #version NUMBER and make changes to the source according to current OpenGL version.
    Q_INVOKABLE static QString processSource(const QString& source);

    // Get pixel format for single and double component textures.
    static std::tuple<int, int> getTexImageFormats();

    // Prepend #version NUMBER and make changes to the source according to current OpenGL version.
    static QByteArray modernizeShaderSource(const QByteArray& source);

    static void registerQmlType();
};

QByteArray preprocessShaderSource(const char* parenthesized);

} // namespace nx::vms::client::core

#define QN_SHADER_SOURCE(...) \
    nx::vms::client::core::graphics::preprocessShaderSource(BOOST_PP_STRINGIZE((__VA_ARGS__)))
