#include "software_trigger_pixmaps.h"

#include <set>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>

#include <ui/style/skin.h>
#include <nx/utils/string.h>

namespace {

static const QString kPixmapExtension = lit(".png");

QPixmap getTriggerPixmap(const QString& name)
{
    if (name.isEmpty())
        return QPixmap();

    return qnSkin->pixmap(QnSoftwareTriggerPixmaps::pixmapsPath() + name + kPixmapExtension, true);
}

} // namespace

QString QnSoftwareTriggerPixmaps::pixmapsPath()
{
    return lit("soft_triggers/user_selectable") + QDir::separator();
}

QString QnSoftwareTriggerPixmaps::defaultPixmapName()
{
    return lit("_bell_on");
}

QString QnSoftwareTriggerPixmaps::effectivePixmapName(const QString& name)
{
    return hasPixmap(name) ? name : defaultPixmapName();
}

const QStringList& QnSoftwareTriggerPixmaps::pixmapNames()
{
    static const QStringList pixmapNames =
        []() -> QStringList
        {
            std::set<QString, decltype(&nx::utils::naturalStringLess)> names(
                &nx::utils::naturalStringLess);

            QRegExp suffixesToSkip(QLatin1String("(_hovered|_pressed|_selected|_disabled|@[0-9]+x)$"));

            for (const auto path: qnSkin->paths())
            {
                const auto fullPath = path + QDir::separator() + pixmapsPath();
                const QStringList pixmapFilter({ lit("*") + kPixmapExtension });

                const auto entries = QDir(fullPath).entryList(pixmapFilter, QDir::Files);

                for (const auto entry: entries)
                {
                    const auto name = QFileInfo(entry).baseName();
                    if (suffixesToSkip.indexIn(name) == -1)
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

bool QnSoftwareTriggerPixmaps::hasPixmap(const QString& name)
{
    return !getTriggerPixmap(name).isNull();
}

QPixmap QnSoftwareTriggerPixmaps::pixmapByName(const QString& name)
{
    const auto pixmap = getTriggerPixmap(name);
    return pixmap.isNull()
        ? getTriggerPixmap(defaultPixmapName())
        : pixmap;
}

QPixmap QnSoftwareTriggerPixmaps::colorizedPixmap(const QString& name, const QColor& color)
{
    static QPixmapCache staticCache;

    const auto effectiveName = effectivePixmapName(name);
    const auto key = effectiveName + lit("\n") + color.name();

    QPixmap result;
    if (staticCache.find(key, &result))
        return result;

    result = QnSkin::colorize(getTriggerPixmap(effectiveName), color);

    staticCache.insert(key, result);
    return result;
}
