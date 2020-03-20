#pragma once

#include <tuple>

#include <QtCore/QObject>

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

    static void registerQmlType();
};

} // namespace nx::vms::client::core
