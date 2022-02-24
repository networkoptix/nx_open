// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <settings/show_once.h>

#include <nx/utils/singleton.h>

class QnClientShowOnceSettings:
    public nx::settings::ShowOnce,
    public Singleton<QnClientShowOnceSettings>
{
    Q_OBJECT
    using base_type = nx::settings::ShowOnce;
public:
    QnClientShowOnceSettings(QObject* parent = nullptr);
};

#define qnClientShowOnce QnClientShowOnceSettings::instance()

