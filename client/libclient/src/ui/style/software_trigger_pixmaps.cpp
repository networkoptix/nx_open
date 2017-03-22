#include "software_trigger_pixmaps.h"
#include <ui/style/skin.h>

QString QnSoftwareTriggerPixmaps::pixmapsPath()
{
    return lit("soft_triggers/");
}

QString QnSoftwareTriggerPixmaps::defaultPixmapName()
{
    return lit("_bell_on");
}

const QStringList& QnSoftwareTriggerPixmaps::pixmapNames()
{
    static const QStringList pixmapNames({
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

    return pixmapNames;
}

QPixmap QnSoftwareTriggerPixmaps::pixmapByName(const QString& name)
{
    const auto pixmap = qnSkin->pixmap(pixmapsPath() + name + lit(".png"));
    return pixmap.isNull()
        ? pixmapByName(defaultPixmapName())
        : pixmap;
}
