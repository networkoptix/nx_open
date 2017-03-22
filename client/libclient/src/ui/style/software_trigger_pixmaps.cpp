#include "software_trigger_pixmaps.h"

#include <set>
#include <ui/style/skin.h>
#include <nx/utils/string.h>

namespace {

static const QString kPixmapExtension = lit(".png");
static const QString kPixmapFilter = lit("*") + kPixmapExtension;

QPixmap getTriggerPixmap(const QString& name)
{
    return qnSkin->pixmap(QnSoftwareTriggerPixmaps::pixmapsPath() + name + kPixmapExtension);
}

} // namespace

QString QnSoftwareTriggerPixmaps::pixmapsPath()
{
    return lit("soft_triggers") + QDir::separator();
}

QString QnSoftwareTriggerPixmaps::defaultPixmapName()
{
    return lit("_bell_on");
}

const QStringList& QnSoftwareTriggerPixmaps::pixmapNames()
{
    static const QStringList pixmapNames =
        []() -> QStringList
        {
            std::set<QString, decltype(&nx::utils::naturalStringLess)> names(
                &nx::utils::naturalStringLess);

            QRegExp suffixesToSkip(lit("(_hovered|_pressed|_selected|_disabled|@[0-9]+x)$"));

            for (const auto path: qnSkin->paths())
            {
                const auto fullPath = path + QDir::separator() + pixmapsPath();
                const auto entries = QDir(fullPath).entryList({ kPixmapFilter }, QDir::Files);

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
