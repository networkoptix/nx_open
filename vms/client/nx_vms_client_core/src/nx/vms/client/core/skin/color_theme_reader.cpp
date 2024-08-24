// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_theme_reader.h"

#include <QtCore/QRegularExpression>

#include <nx/fusion/serialization/json_functions.h>
#include <utils/common/hash.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/qt_helpers.h>

namespace nx::vms::client::core {

namespace {

void sortColorsInGroups(ColorTree::ColorGroups* groups)
{
    for (auto& colors: *groups)
    {
        std::sort(colors.begin(), colors.end(),
            [](const QColor& c1, const QColor& c2)
            {
                return c1.toHsl().lightness() < c2.toHsl().lightness();
            });
    }
}

const static QStringList kColorSuffixes = {"_l", "_d", "_bg", "_core"};

} // namespace

// ColorThemeReader ===============================================================================

ColorThemeReader::ColorThemeReader():
    m_colorTree(new MetaColorNode)
{
    m_evaluator.registerFunctions();
    m_evaluator.setResolver(this);
}

ColorThemeReader::~ColorThemeReader()
{
}

QVariant ColorThemeReader::resolveConstant(const QString& /*name*/) const
{
    return QColor();
}

void ColorThemeReader::addColors(const QJsonObject& json)
{
    cleanLinkedColors(m_colorTree);
    deserialize(json, m_colorTree);
    dereferenceLinks(m_colorTree);
}

ColorTree ColorThemeReader::getColorTree() const
{
    return ColorTree(m_colorTree);
}

void ColorThemeReader::deserialize(const QJsonObject& json,
    ColorThemeReader::MetaColorNode::Ptr currentNode)
{
    NX_ASSERT(currentNode);

    for (auto it = json.begin(); it != json.end(); ++it)
    {
        const QString nodeName = it.key();

        if (nodeName == "_")
            continue;

        auto newNodeIt = currentNode->children.find(nodeName);
        if (newNodeIt == currentNode->children.end())
            newNodeIt = currentNode->children.insert(nodeName, MetaColorNode::Ptr(new MetaColorNode));
        else
            newNodeIt.value()->colors.clear();

        MetaColorNode::Ptr childNode = newNodeIt.value();

        if (it.value().type() == QJsonValue::Object)
        {
            deserialize(it.value().toObject(), childNode);
        }
        else if (it.value().type() == QJsonValue::String)
        {
            const QString textValue = it.value().toString();
            const QColor value = evaluateColor(textValue);

            childNode->colors.append({value, textValue, value.isValid() ? "" : textValue});
        }
        else if (it.value().type() == QJsonValue::Array)
        {
            for (const auto jsonValue: it.value().toArray())
            {
                const QString textValue = jsonValue.toString();
                const QColor value = evaluateColor(textValue);

                childNode->colors.append({value, textValue, value.isValid() ? "" : textValue});
            }
        }
        else
        {
            NX_ASSERT(false, "Unexpected json object type '%1'.", it.value().type());
        }
    }
}

QColor ColorThemeReader::evaluateColor(const QString& textValue)
{
    // Qee::Parser cannot parse expressions like timeline.clockText.setAlpha(110)
    if (textValue.count(".") != textValue.count("("))
        return QColor();

    QVariant result;

    try
    {
        result = m_evaluator.evaluate(Qee::Parser::parse(textValue));
    }
    catch(const QnException& exception)
    {
        NX_ASSERT(false, exception.what());
    }

    if (static_cast<QMetaType::Type>(result.type()) == QMetaType::QColor)
        return qvariant_cast<QColor>(result);

    return QColor();
}

void ColorThemeReader::dereferenceLinks(ColorThemeReader::MetaColorNode::Ptr currentNode)
{
    NX_ASSERT(currentNode);

    for (auto& node: currentNode->children)
    {
        if (!node->children.isEmpty())
        {
            dereferenceLinks(node);
        }
        else
        {
            node->valueEvaluatingInProgress = true;

            for (int i = 0; i < node->colors.size(); ++i)
            {
                auto& colorDescription = node->colors[i];
                if (colorDescription.isLink() && !colorDescription.value.isValid())
                    dereferenceNodeColor(colorDescription);
            }

            node->valueEvaluatingInProgress = false;
        }
    }
}

QString ColorThemeReader::dereferenceLink(const QString& link)
{
    const QStringList linkParts = link.split(".");

    MetaColorNode::Ptr node = m_colorTree;
    QString lastLinkPart = "";

    for (const QString& linkPart: linkParts)
    {
        auto nextNodeIt = node->children.find(linkPart);
        if (nextNodeIt != node->children.end())
            node = nextNodeIt.value();
        else
            lastLinkPart = linkPart;
    }

    if (node->colors.isEmpty())
    {
        NX_ASSERT(false, "Invalid text link value: " + link);
        return QString();
    }

    if (node->colors.size() > 1)
    {
        NX_ASSERT(false, "Multiple color definition.");
        return QString();
    }

    auto& colorDescription = node->colors.first();
    if (!colorDescription.value.isValid())
    {
        // cyclic color definition check
        if (node->valueEvaluatingInProgress)
        {
            NX_ASSERT(false, "Cyclic color reference.");
            return QString();
        }

        dereferenceNodeColor(colorDescription);
    }

    return lastLinkPart.isEmpty()
        ? colorDescription.textValue
        : colorDescription.textValue + "." + lastLinkPart;
}

void ColorThemeReader::dereferenceNodeColor(
    ColorThemeReader::MetaColorNode::ColorDescription& colorDescription)
{
    colorDescription.textValue = dereferenceLink(colorDescription.link);
    colorDescription.value = evaluateColor(colorDescription.textValue);
}

void ColorThemeReader::cleanLinkedColors(ColorThemeReader::MetaColorNode::Ptr currentNode)
{
    for (auto it = currentNode->children.begin(); it != currentNode->children.end(); ++it)
    {
        for (int i = 0; i < it.value()->colors.size(); ++i)
        {
            if (it.value()->colors[i].isLink())
            {
                it.value()->colors[i].value = QColor();
                it.value()->colors[i].textValue.clear();
            }
        }

        cleanLinkedColors(it.value());
    }
}

// ColorTree ======================================================================================

using ColorData = ColorTree::ColorData;
using ColorMap = ColorTree::ColorMap;

namespace {

ColorMap recursiveConversionToColorMap(const ColorTree::Node::Ptr parentNode, QString prefix)
{
    if (!NX_ASSERT(parentNode) || parentNode->children.empty())
        return {};

    ColorMap result;

    for (const auto& [name, node]: nx::utils::constKeyValueRange(parentNode->children))
    {
        switch (node->colors.size())
        {
            // Color value.
            case 1:
                result[prefix + name] = node->colors[0];
                break;

            // Map.
            case 0:
                result.insert(recursiveConversionToColorMap(node, prefix + name + "."));
                break;

            // Array.
            default:
            {
                result[prefix + name] = node->colors;
                break;
            }
        }
    }

    return result;
}

QVariantMap recursiveConversionToVariantMap(const ColorTree::Node::Ptr parentNode)
{
    if (!NX_ASSERT(parentNode) || parentNode->children.empty())
        return {};

    QVariantMap result;

    for (const auto& [name, node]: nx::utils::constKeyValueRange(parentNode->children))
    {
        switch (node->colors.size())
        {
            // Color value.
            case 1:
                result[name] = node->colors[0];
                break;

            // Map.
            case 0:
                result[name] = recursiveConversionToVariantMap(node);
                break;

            // Array.
            default:
            {
                QVariantList list;
                std::transform(node->colors.cbegin(), node->colors.cend(), std::back_inserter(list),
                    [](QColor color) { return QVariant::fromValue(color); });

                result[name] = list;
            }
        }
    }

    return result;
}

} // namespace

ColorTree::ColorTree(const ColorThemeReader::MetaColorNode::Ptr metaTree):
    m_root(new Node())
{
    copyNodes(m_root, metaTree);
}

ColorTree::ColorTree(ColorTree&& other) noexcept:
    m_root(std::move(other.m_root))
{}

ColorTree& ColorTree::operator=(ColorTree&& other) noexcept
{
    m_root = std::move(other.m_root);
    return *this;
}

QMap<QString, QColor> ColorTree::getRootColors() const
{
    QMap<QString, QColor> result;

    for (auto [nodeName, node]: nx::utils::constKeyValueRange(m_root->children))
    {
        if (node->colors.size() == 1)
            result[nodeName] = node->colors[0];
    }

    return result;
}

ColorSubstitutions ColorTree::rootColorsDelta(const ColorTree& other) const
{
    ColorSubstitutions result;

    const QMap<QString, QColor> thisRootColors = getRootColors();
    const QMap<QString, QColor> otherRootColors = other.getRootColors();

    const QSet<QString> commonKeys = nx::utils::toQSet(thisRootColors.keys())
        .intersect(nx::utils::toQSet(otherRootColors.keys()));

    for (const QString& key: commonKeys)
    {
        const QColor& thisColor = thisRootColors[key];
        const QColor& otherColor = otherRootColors[key];

        if (thisColor.isValid() && otherColor.isValid() && (thisColor != otherColor))
            result.insert(thisColor, otherColor);
    }

    return result;
}

ColorMap ColorTree::colorsByPath() const
{
    return recursiveConversionToColorMap(m_root, "");
}

QVariantMap ColorTree::toVariantMap() const
{
    return recursiveConversionToVariantMap(m_root);
}

void ColorTree::baseColorGroups(ColorGroups* groups, QHash<QColor, ColorInfo>* colorInfo) const
{
    *groups = createBaseColorGroups();

    sortColorsInGroups(groups);

    for (auto it = groups->begin(); it != groups->end(); ++it)
    {
        auto& colors = it.value();
        for (int i = 0; i < colors.size(); ++i)
            colorInfo->insert(colors[i], ColorInfo{it.key(), i});
    }
}

void ColorTree::copyNodes(ColorTree::Node::Ptr parentNode,
    const ColorThemeReader::MetaColorNode::Ptr metaNode) const
{
    for (auto metaNodeIt = metaNode->children.begin(); metaNodeIt != metaNode->children.end(); ++metaNodeIt)
    {
        ColorTree::Node::Ptr newNode(new ColorTree::Node());

        for (const auto& colorDescription: metaNodeIt.value()->colors)
        {
            if (colorDescription.value.isValid())
                newNode->colors.append(colorDescription.value);
        }

        parentNode->children[metaNodeIt.key()] = newNode;
        copyNodes(newNode, metaNodeIt.value());
    }
}

ColorTree::ColorGroups ColorTree::createBaseColorGroups() const
{
    ColorGroups result;

    const QRegularExpression groupNameRe("([^\\d]+)");

    const QMap<QString, QColor> rootColors = getRootColors();
    for (auto it = rootColors.begin(); it != rootColors.end(); ++it)
    {
        const auto& colorName = it.key();
        const auto& color = it.value();

        if (auto groupNameMatch = groupNameRe.match(colorName); groupNameMatch.hasMatch())
        {
            QString groupName = groupNameMatch.captured(0);
            for (const auto& suffix: kColorSuffixes)
            {
                if (groupName.endsWith(suffix))
                    groupName.chop(suffix.size());
            }

            auto& group = result[groupName];
            group.append(color);
        }
    }

    return result;
}

} // namespace nx::vms::client::core
