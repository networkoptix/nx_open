// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "svg_icon_colorer.h"

#include <QFileInfo>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/common/color_substitutions.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include "icon.h"
#include "icon_loader.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr int kColorValueStringLength = 7;

static const QColor kBasePrimaryColor = "#a5b7c0"; //< Value of light10 in default customization.
static const QColor kBaseSecondaryColor = "#e1e7ea"; //< Value of light4 in default customization.
static const QColor kBaseWindowTextColor = "#698796"; //< Value of light16 ('windowText').

int getNextColorPos(const QByteArray& data, int startPos)
{
    for (; startPos < data.size(); ++startPos)
    {
        if (data[startPos] == '#')
        {
            return data.size() - startPos >= kColorValueStringLength
                ? startPos
                : data.size();
        }
    }

    return startPos;
}

ColorSubstitutions colorMapFromStringMap(const SvgIconColorer::Substitutions& stringMap)
{
    ColorSubstitutions result;
    for (auto it = stringMap.begin(); it != stringMap.end(); ++it)
        result[it.key()] = colorTheme()->color(it.value());
    return result;
}

QByteArray substituteColors(
    const QByteArray& data,
    const ColorSubstitutions& colorSubstitutions)
{
    QByteArray result = data;

    for (int pos = 0; pos < data.size(); ++pos)
    {
        pos = getNextColorPos(data, pos);
        if (pos != data.size())
        {
            QString currentColor = QString::fromLatin1(
                data.begin() + pos,
                kColorValueStringLength).toLower();

            if (colorSubstitutions.contains(currentColor))
            {
                result.replace(
                    pos,
                    kColorValueStringLength,
                    colorSubstitutions[currentColor].name().toLatin1());
            }

            pos += kColorValueStringLength;
        }
    }

    return result;
}

} // namespace

const SvgIconColorer::IconSubstitutions SvgIconColorer::kDefaultIconSubstitutions = {
    { QnIcon::Disabled, {
        { kBasePrimaryColor, "dark14" },
        { kBaseSecondaryColor, "dark17" },
        { kBaseWindowTextColor, "light16" },
    }},
    { QnIcon::Selected, {
        { kBasePrimaryColor, "light4" },
        { kBaseSecondaryColor, "light1" },
        { kBaseWindowTextColor, "light10" },
    }},
    { QnIcon::Active, {
        { kBasePrimaryColor, "brand_core" },
        { kBaseSecondaryColor, "brand_l2" },
        { kBaseWindowTextColor, "light14" },
    }},
    { QnIcon::Error, {
        { kBasePrimaryColor, "red_l2" },
        { kBaseSecondaryColor, "red_l3" },
        { kBaseWindowTextColor, "light16" },
    }},
};

SvgIconColorer::SvgIconColorer(
    const QByteArray& sourceIconData,
    const QString& iconName,
    const IconSubstitutions& substitutions /*= kDefaultIconSubstitutions*/):
    m_sourceIconData(sourceIconData),
    m_iconName(iconName),
    m_substitutions(substitutions)
{
    initializeDump();
    dumpIconIfNeeded(sourceIconData);
}

QByteArray SvgIconColorer::colorized(const QByteArray& source)
{
    return substituteColors(source, colorTheme()->getColorSubstitutions());
}

void SvgIconColorer::initializeDump()
{
    const QString directoryPath(ini().dumpGeneratedIconsTo);
    if (directoryPath.isEmpty())
        return;

    const QFileInfo basePath = QDir(directoryPath).absoluteFilePath(m_iconName);

    m_dumpDirectory = basePath.absoluteDir();
    m_iconName = basePath.baseName();
    m_dump = m_dumpDirectory.mkpath(m_dumpDirectory.path());
}

void SvgIconColorer::dumpIconIfNeeded(const QByteArray& data, const QString& suffix) const
{
    static const QString svgExtension(".svg");

    if (!m_dump)
        return;

    QString filename = m_dumpDirectory.absoluteFilePath(m_iconName + suffix + svgExtension);
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
        return;

    f.write(data);
    f.close();
}

QByteArray SvgIconColorer::makeIcon(QIcon::Mode mode) const
{
    const ColorSubstitutions substitutions = (mode == QnIcon::Normal)
        ? colorTheme()->getColorSubstitutions()
        : colorMapFromStringMap(m_substitutions.value(mode));

    const QString suffix = (mode == QnIcon::Normal)
        ? ""
        : ("_" + IconLoader::kDefaultSuffixes.value(mode));

    QByteArray result = substituteColors(m_sourceIconData, substitutions);
    dumpIconIfNeeded(result, suffix);

    return result;
}

SvgIconColorer::IconSubstitutions SvgIconColorer::IconSubstitutions::operator+(
    const SvgIconColorer::IconSubstitutions& other) const
{
    IconSubstitutions result = *this;
    for (IconSubstitutions::const_iterator mode = other.begin(); mode != other.end(); ++mode)
    {
        const Substitutions& src = mode.value();
        Substitutions& dst = result[mode.key()];

        // Qt 5.15 will have QMap::insert(QMap<Key, T>&) for this.
        for (Substitutions::const_iterator color = src.begin(); color != src.end(); ++color)
            dst[color.key()] = color.value();
    }
    return result;
}

} // namespace nx::vms::client::desktop
