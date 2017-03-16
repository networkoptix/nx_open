#include "software_trigger_icons.h"
#include <ui/style/skin.h>

QIcon QnSoftwareTriggerIcons::iconByName(const QString& name)
{
    const auto icon = qnSkin->icon(iconsPath() + name + lit(".png"));
    return icon.isNull()
        ? iconByName(defaultIconName())
        : icon;
}

QString QnSoftwareTriggerIcons::iconsPath()
{
    return lit("triggers/");
}

QString QnSoftwareTriggerIcons::defaultIconName()
{
    //TODO: #vkutin Specify default (fallback) icon.
    return lit("mic");
}

const QStringList& QnSoftwareTriggerIcons::iconNames()
{
    //TODO: #vkutin #common Fill this with software trigger icon names.

    static const QStringList iconNames({
        lit("mic") });

    return iconNames;
}
