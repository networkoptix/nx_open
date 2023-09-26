// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtGui/QIcon>

class QnIcon: public QIcon
{
public:
    static inline const Mode Error = static_cast<Mode>(0xE);
    static inline const Mode Pressed = static_cast<Mode>(0xF);

    using Suffixes = QMap<Mode, QString>;
};
