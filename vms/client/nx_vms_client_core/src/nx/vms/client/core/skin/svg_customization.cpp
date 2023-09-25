// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "svg_customization.h"

#include <QtXml/QDomElement>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/algorithm.h>

#include "color_substitutions.h"

namespace nx::vms::client::core {

namespace {

static constexpr int kColorValueStringLength = 7;

static const QColor kFallbackColor =
    build_info::publicationType() == build_info::PublicationType::release
        ? Qt::transparent
        : Qt::magenta;

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

} // namespace

QByteArray customizeSvgColorClasses(const QByteArray& sourceData,
    const QMap<QString /*source class name*/, QColor /*target color*/>& customization,
    const QString& svgName /*< for debug output*/,
    QSet<QString>* classesMissingCustomization)
{
    QDomDocument doc;
    if (const auto res = doc.setContent(sourceData); !res)
    {
        NX_ASSERT(false, "Cannot parse svg icon \"%1\": %2", svgName, res.errorMessage);
        return sourceData;
    }

    QSet<QString> missingClasses;

    const auto updateElement =
        [&](QDomElement element)
        {
            const auto className = element.attribute("class", "");
            if (className.isEmpty())
                return;

            const bool hasColor = customization.contains(className);
            if (!hasColor)
                missingClasses.insert(className);

            auto targetColor = hasColor
                ? customization.value(className)
                : QColor{};

            if (!targetColor.isValid())
                targetColor = kFallbackColor;

            if (targetColor.alphaF() < 1.0)
                element.setAttribute("opacity", targetColor.alphaF());

            element.setAttribute("fill", targetColor.name(QColor::NameFormat::HexRgb).toLatin1());
        };

    const auto recursiveDfs = nx::utils::y_combinator(
        [updateElement](const auto recursiveDfs, QDomElement element) -> void
        {
            updateElement(element);
            const auto children = element.childNodes();
            for (int i = 0; i < children.size(); ++i)
                recursiveDfs(children.at(i).toElement());
        });

    recursiveDfs(doc.documentElement());

    if (classesMissingCustomization)
    {
        *classesMissingCustomization = missingClasses;
    }
    else
    {
        NX_ASSERT(missingClasses.empty(), "Customization is missing for the following classes in "
            "svg image \"%1\": %2", svgName, missingClasses);
    }

    return doc.toByteArray();
}

QByteArray substituteSvgColors(const QByteArray& sourceData,
    const QMap<QColor /*source color*/, QColor /*target color*/>& substitutions)
{
    if (substitutions.empty())
        return sourceData;

    QByteArray result = sourceData;
    for (int pos = 0; pos < result.size(); ++pos)
    {
        pos = getNextColorPos(result, pos);
        if (pos != result.size())
        {
            const auto sourceColor = QColor::fromString(QAnyStringView(
                result.begin() + pos,
                kColorValueStringLength));

            if (sourceColor.isValid() && substitutions.contains(sourceColor))
            {
                const auto targetColor = substitutions.value(sourceColor);
                result.replace(
                    pos,
                    kColorValueStringLength,
                    targetColor.name(QColor::NameFormat::HexRgb).toLatin1());

                if (targetColor.alphaF() < 1.0)
                {
                    const auto opacity = QString(" opacity=\"%1\"").arg(targetColor.alphaF());
                    result.insert(pos + kColorValueStringLength, opacity.toLatin1());
                    pos += opacity.length();
                }
            }

            pos += kColorValueStringLength;
        }
    }

    return result;
}

} // namespace nx::vms::client::core
