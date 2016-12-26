#pragma once

#include <iostream>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/network/socket_common.h>

void PrintTo(const QByteArray& val, ::std::ostream* os);
void PrintTo(const QString& val, ::std::ostream* os);
void PrintTo(const SocketAddress& val, ::std::ostream* os);
void PrintTo(const QUrl& val, ::std::ostream* os);
