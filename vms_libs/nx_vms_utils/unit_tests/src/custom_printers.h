#pragma once

#include <iostream>

#include <QByteArray>
#include <QString>

//!Used by google test to print QByteArray as text
void PrintTo(const QByteArray& val, ::std::ostream* os);
void PrintTo(const QString& val, ::std::ostream* os);
