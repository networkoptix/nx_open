// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "svg_icon_colorer.h"

#include <functional>

#include <QtCore/QCryptographicHash>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtXml/QDomElement>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/skin/color_substitutions.h>
#include <nx/vms/client/core/skin/color_theme.h>

#include "icon.h"
#include "icon_loader.h"

namespace nx::vms::client::core {

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
    const ColorSubstitutions& colorSubstitutions,
    const SvgIconColorer::ThemeColorsRemapData& themeSubstitutions)
{
    QByteArray result = data;

    if (!colorSubstitutions.empty())
    {
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
    }

    if (themeSubstitutions.empty())
        return result;

    QDomDocument doc;
    if (auto res = doc.setContent(result); !res)
    {
        NX_ASSERT(false, "Cannot parse svg icon: %1", res.errorMessage);
        return data;
    }

    const auto updateElement =
        [&themeSubstitutions](QDomElement element)
        {
            if (auto _class = element.attribute("class", "");
                themeSubstitutions.contains(_class))
            {
                QColor value;
                if (NX_ASSERT(themeSubstitutions[_class] != kInvalidColor,
                    "Color theme is missing for icon, %1 is not specified", _class))
                {
                    value = colorTheme()->color(themeSubstitutions[_class]);
                }
                else
                {
                    // Use special value to make invalid icons noticeable in the dev builds.
                    value = build_info::publicationType() == build_info::PublicationType::release
                        ? Qt::transparent
                        : Qt::magenta;
                }

                if (themeSubstitutions.alpha < 1.0)
                    element.setAttribute("opacity", themeSubstitutions.alpha);
                element.setAttribute("fill", value.name().toLatin1());
            }
        };

    auto recursiveDfs = nx::utils::y_combinator(
        [updateElement](auto recursiveDfs, QDomElement element) -> void
        {
            updateElement(element);
            auto children = element.childNodes();
            for (int i = 0; i < children.size(); ++i)
                recursiveDfs(children.at(i).toElement());
        });

    recursiveDfs(doc.documentElement());
    return doc.toByteArray();
}

} // namespace

static const SvgIconColorer::ThemeColorsRemapData kUnspecified;

const SvgIconColorer::IconSubstitutions SvgIconColorer::kTreeIconSubstitutions = {
    { QIcon::Disabled, {
        { kBasePrimaryColor, "dark14" },
        { kBaseSecondaryColor, "dark17" },
        { kBaseWindowTextColor, "light16" },
    }},
    { QIcon::Selected, {
        { kBasePrimaryColor, "light4" },
        { kBaseSecondaryColor, "light1" },
        { kBaseWindowTextColor, "light10" },
    }},
    { QIcon::Active, {  //< Hovered
        { kBasePrimaryColor, "brand_core" },
        { kBaseSecondaryColor, "light3" },
        { kBaseWindowTextColor, "light14" },
    }},
    { QnIcon::Error, {
        { kBasePrimaryColor, "red_l2" },
        { kBaseSecondaryColor, "red_l3" },
        { kBaseWindowTextColor, "light16" },
    }},
};

const SvgIconColorer::IconSubstitutions SvgIconColorer::kDefaultIconSubstitutions = {};

SvgIconColorer::SvgIconColorer(
    const QByteArray& sourceIconData,
    const QString& iconName,
    const IconSubstitutions& substitutions, /*= kDefaultIconSubstitutions*/
    const QMap<QIcon::Mode, SvgIconColorer::ThemeColorsRemapData>& themeSubstitutions):
    m_sourceIconData(sourceIconData),
    m_iconName(iconName),
    m_substitutions(substitutions),
    m_themeSubstitutions(themeSubstitutions)
{
    initializeDump();
    dumpIconIfNeeded(sourceIconData);
}

QByteArray SvgIconColorer::colorized(const QByteArray& source)
{
    return substituteColors(source, colorTheme()->getColorSubstitutions(), {});
}

void SvgIconColorer::initializeDump()
{
    const QString directoryPath(ini().dumpGeneratedIconsTo);
    if (directoryPath.isEmpty())
        return;

    const QFileInfo basePath(QDir(directoryPath).absoluteFilePath(m_iconName));

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
    const ColorSubstitutions substitutions =
        (mode == QnIcon::Normal && !m_substitutions.contains(mode))
            ? colorTheme()->getColorSubstitutions()
            : colorMapFromStringMap(m_substitutions.value(mode));

    const ThemeColorsRemapData themeSubstitutions = m_themeSubstitutions.value(mode, kUnspecified);

    const QString suffix = (mode == QnIcon::Normal)
        ? ""
        : ("_" + IconLoader::kDefaultSuffixes.value(mode));

    QByteArray result = substituteColors(m_sourceIconData, substitutions, themeSubstitutions);

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

QString SvgIconColorer::IconSubstitutions::hash() const
{
    QByteArray data;
    QDataStream stream(&data, QDataStream::WriteOnly);
    stream << *this;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(data);
    return QString::fromLatin1(hash.resultView());
}

QString SvgIconColorer::ThemeSubstitutions::hash() const
{
    QByteArray data;
    QDataStream stream(&data, QDataStream::WriteOnly);
    for (const auto& key: this->keys())
        stream << key << "/" << this->value(key).toString() << "/";
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(data);
    return QString::fromLatin1(hash.resultView());
}

} // namespace nx::vms::client::core
