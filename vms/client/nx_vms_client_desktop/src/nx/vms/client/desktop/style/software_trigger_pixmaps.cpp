// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_pixmaps.h"

#include <set>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>

#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/skin.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kPixmapExtension = ".png";

QPixmap getTriggerPixmap(const QString& name)
{
    if (name.isEmpty())
        return QPixmap();

    return qnSkin->pixmap(SoftwareTriggerPixmaps::pixmapsPath() + name + kPixmapExtension, true);
}

} // namespace

QString SoftwareTriggerPixmaps::pixmapsPath()
{
    return QString("soft_triggers/user_selectable") + QDir::separator();
}

QString SoftwareTriggerPixmaps::defaultPixmapName()
{
    return "_bell_on";
}

QString SoftwareTriggerPixmaps::effectivePixmapName(const QString& name)
{
    return hasPixmap(name) ? name : defaultPixmapName();
}

QString SoftwareTriggerPixmaps::effectivePixmapPath(const QString& name)
{
    return pixmapsPath() + effectivePixmapName(name) + kPixmapExtension;
}

const QStringList& SoftwareTriggerPixmaps::pixmapNames()
{
    static const QStringList pixmapNames =
        []() -> QStringList
        {
            std::set<QString, decltype(&nx::utils::naturalStringLess)> names(
                &nx::utils::naturalStringLess);

            QRegularExpression suffixesToSkip("(_hovered|_pressed|_selected|_disabled|@[0-9]+x)$");

            for (const auto path: qnSkin->paths())
            {
                const auto fullPath = path + QDir::separator() + pixmapsPath();
                const QStringList pixmapFilter({ "*" + kPixmapExtension });

                const auto entries = QDir(fullPath).entryList(pixmapFilter, QDir::Files);

                for (const auto entry: entries)
                {
                    const auto name = QFileInfo(entry).baseName();
                    if (!suffixesToSkip.match(name).hasMatch())
                        names.insert(name);
                }
            }

            QStringList result;
            for (const auto name: names)
                result.push_back(name);

            return result;
        }();

    return pixmapNames;
}

bool SoftwareTriggerPixmaps::hasPixmap(const QString& name)
{
    return !getTriggerPixmap(name).isNull();
}

QPixmap SoftwareTriggerPixmaps::pixmapByName(const QString& name)
{
    const auto pixmap = getTriggerPixmap(name);
    return pixmap.isNull()
        ? getTriggerPixmap(defaultPixmapName())
        : pixmap;
}

QPixmap SoftwareTriggerPixmaps::colorizedPixmap(const QString& name, const QColor& color)
{
    static QPixmapCache staticCache;

    const auto effectiveName = effectivePixmapName(name);
    const auto key = effectiveName + "\n" + color.name();

    QPixmap result;
    if (staticCache.find(key, &result))
        return result;

    result = core::Skin::colorize(getTriggerPixmap(effectiveName), color);

    staticCache.insert(key, result);
    return result;
}

} // namespace nx::vms::client::desktop
