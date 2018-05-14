#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSize>

namespace dshow{
namespace utils{

    QList<QString> listDevices();
    QList<QSize> listResolutions();

} // namespace utils
} // namespace dshow