#include "software_trigger_icons.h"
#include <ui/style/skin.h>

QString QnSoftwareTriggerIcons::iconsPath()
{
    return lit("soft_triggers/");
}

QString QnSoftwareTriggerIcons::defaultIconName()
{
    return lit("_bell_on");
}

const QStringList& QnSoftwareTriggerIcons::iconNames()
{
    static const QStringList iconNames({
        lit("_lights_on"),
        lit("_lights_off"),
        lit("_alarm_on"),
        lit("_alarm_off"),
        lit("_bell_on"),
        lit("_bell_off"),
        lit("_door_opened"),
        lit("_door_closed"),
        lit("_lock_locked"),
        lit("_lock_unlocked"),
        lit("_mark_yes"),
        lit("_mark_no"),
        lit("ban"),
        lit("bookmark"),
        lit("bubble"),
        lit("circle"),
        lit("message"),
        lit("mic"),
        lit("puzzle"),
        lit("siren"),
        lit("speaker")
    });

    return iconNames;
}

QPixmap QnSoftwareTriggerIcons::pixmapByName(const QString& name)
{
    const auto pixmap = qnSkin->pixmap(iconsPath() + name + lit(".png"));
    return pixmap.isNull()
        ? pixmapByName(defaultIconName())
        : pixmap;
}

QIcon QnSoftwareTriggerIcons::iconByName(const QString& name)
{
    const auto icon = qnSkin->icon(iconsPath() + name + lit(".png"));
    return icon.isNull()
        ? iconByName(defaultIconName())
        : icon;
}
