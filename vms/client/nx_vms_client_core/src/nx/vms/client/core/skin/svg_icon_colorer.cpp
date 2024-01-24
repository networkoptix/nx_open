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

const SvgIconColorer::Substitutions kMapFromOldColorTheme = {
        { QColor("#000000"), "dark1" },
        { QColor("#080707"), "dark2" },
        { QColor("#0d0e0f"), "dark3" },
        { QColor("#121517"), "dark4" },
        { QColor("#171c1f"), "dark5" },
        { QColor("#1c2327"), "dark6" },
        { QColor("#212a2f"), "dark7" },
        { QColor("#263137"), "dark8" },
        { QColor("#2b383f"), "dark9" },
        { QColor("#303f47"), "dark10" },
        { QColor("#35464f"), "dark11" },
        { QColor("#3a4d57"), "dark12" },
        { QColor("#3f545f"), "dark13" },
        { QColor("#445b67"), "dark14" },
        { QColor("#49626f"), "dark15" },
        { QColor("#4e6977"), "dark16" },
        { QColor("#53707f"), "dark17" },

        { QColor("#ffffff"), "light1" },
        { QColor("#f5f7f8"), "light2" },
        { QColor("#ebeff1"), "light3" },
        { QColor("#e1e7ea"), "light4" },
        { QColor("#d7dfe3"), "light5" },
        { QColor("#cdd7dc"), "light6" },
        { QColor("#c3cfd5"), "light7" },
        { QColor("#b9c7ce"), "light8" },
        { QColor("#afbfc7"), "light9" },
        { QColor("#a5b7c0"), "light10" },
        { QColor("#9bafb9"), "light11" },
        { QColor("#91a7b2"), "light12" },
        { QColor("#879fab"), "light13" },
        { QColor("#7d97a4"), "light14" },
        { QColor("#738f9d"), "light15" },
        { QColor("#698796"), "light16" },
        { QColor("#5f7f8f"), "light17" },

        { QColor("#77d2ff"), "brand_l4" },
        { QColor("#5ecbff"), "brand_l3" },
        { QColor("#43c2ff"), "brand_l2" },
        { QColor("#39b2ef"), "brand_l1" },
        { QColor("#2fa2db"), "brand_core" },
        { QColor("#2592c3"), "brand_d1" },
        { QColor("#1b82ad"), "brand_d2" },
        { QColor("#117297"), "brand_d3" },
        { QColor("#076281"), "brand_d4" },
        { QColor("#045773"), "brand_d5" },
        { QColor("#054A61"), "brand_d6" },
        { QColor("#043E51"), "brand_d7" },

        { QColor("#43c2ff"), "blue8" },
        { QColor("#39b2ef"), "blue9" },
//        { QColor("#2fa2db"), "blue10" }, //< Matches brand_core.
        { QColor("#2592c3"), "blue11" },
        { QColor("#1b82ad"), "blue12" },
        { QColor("#117297"), "blue13" },
        { QColor("#076281"), "blue14" },

        { QColor("#56e829"), "green_l4" },
        { QColor("#51d22a"), "green_l3" },
        { QColor("#4cbc28"), "green_l2" },
        { QColor("#44a624"), "green_l1" },
        { QColor("#3a911e"), "green_core" },
        { QColor("#32731e"), "green_d1" },
        { QColor("#2a551e"), "green_d2" },
        { QColor("#22391e"), "green_d3" },

        { QColor("#fbbc05"), "yellow_core" },
        { QColor("#e1a700"), "yellow_d1" },
        { QColor("#c79200"), "yellow_d2" },

        { QColor("#ff2e2e"), "red_l3" },
        { QColor("#f02c2c"), "red_l2" },
        { QColor("#d92a2a"), "red_l1" },
        { QColor("#c22626"), "red_core" },
        { QColor("#aa1e1e"), "red_d1" },
        { QColor("#8e1717"), "red_d2" },
        { QColor("#741414"), "red_d3" },
        { QColor("#5e1010"), "red_d4" },
        { QColor("#480d0d"), "red_d5" },
        { QColor("#330909"), "red_d6" },
        { QColor("#1d0505"), "red_d7" },
};

const SvgIconColorer::IconSubstitutions SvgIconColorer::kDefaultIconSubstitutions = {
    { QIcon::Normal, kMapFromOldColorTheme },
    { QIcon::Active, kMapFromOldColorTheme },
    { QIcon::Selected, kMapFromOldColorTheme },
    { QIcon::Disabled, kMapFromOldColorTheme },
};

SvgIconColorer::SvgIconColorer(
    const QByteArray& sourceIconData,
    const QString& iconName,
    const IconSubstitutions& substitutions, //< kDefaultIconSubstitutions.
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
