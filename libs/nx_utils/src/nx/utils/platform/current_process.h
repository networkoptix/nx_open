// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>

namespace nx {
namespace utils {

class NX_UTILS_API CurrentProcess
{
public:
    /** linux, osx: changes real and effective user and group IDs to \arg userName
     *  windows: always returns false ^_^ */
    static bool changeUser( const QString& userName );

    /** linux, osx: changes real and effective user and group IDs to \arg userId
     *  windows: always returns false ^_^ */
    static bool changeUser( const uint userId );

private:
    CurrentProcess(); // not constructable
};

} // namespace utils
} // namespace nx
