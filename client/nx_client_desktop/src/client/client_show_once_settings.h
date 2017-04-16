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

