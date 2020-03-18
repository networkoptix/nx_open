#pragma once

#include <atomic>

#include <QtCore/QObject>
#include <QtCore/QString>

namespace nx::vms::client::core::graphics {

class ShaderHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    using base_type::base_type; //< Forward constructors.

    // Prepend #version NUMBER and make changes to the source according to current OpenGL version.
    Q_INVOKABLE QString processSource(const QString& source);

    static void registerQmlType();
};

} // namespace nx::vms::client::core
