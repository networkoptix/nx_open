// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>

#include "color_substitutions.h"
#include "private/evaluator.h"

class QJsonObject;

namespace nx::vms::client::core {

class ColorTree;

/**
 * Json colors parser.
 */
class ColorThemeReader: public Qee::Resolver
{
    // Access to MetaColorNode type
    friend class ColorTree;

    // Auxiliary tree type for json parsing.
    struct MetaColorNode
    {
        using Ptr = QSharedPointer<MetaColorNode>;

        struct ColorDescription
        {
            QColor value;
            QString textValue;
            QString link;

            bool isLink()
            {
                return !link.isEmpty();
            }
        };

        QList<ColorDescription> colors;

        QHash<QString, Ptr> children;

        bool valueEvaluatingInProgress = false;
    };

public:
    explicit ColorThemeReader();
    virtual ~ColorThemeReader() override;

    void addColors(const QJsonObject& json);
    ColorTree getColorTree() const;

protected:
    virtual QVariant resolveConstant(const QString &name) const override;

private:
    void deserialize(const QJsonObject& json, MetaColorNode::Ptr currentNode);
    QColor evaluateColor(const QString& textValue);
    void dereferenceLinks(MetaColorNode::Ptr currentNode);
    QString dereferenceLink(const QString& link);
    void dereferenceNodeColor(MetaColorNode::ColorDescription& description);
    void cleanLinkedColors(MetaColorNode::Ptr currentNode);

private:
    const MetaColorNode::Ptr m_colorTree;

    Qee::Evaluator m_evaluator;
};

struct ColorInfo
{
    QString group;
    int index = -1;

    ColorInfo() = default;
    ColorInfo(const QString& group, int index): group(group), index(index) {}
};

/**
 * Describes colors structure.
 */
class ColorTree
{
public:
    struct Node
    {
        using Ptr = QSharedPointer<Node>;

        QList<QColor> colors;
        QHash<QString, Ptr> children;
    };

    using ColorGroups = QHash<QString, QList<QColor>>;

public:
    ColorTree(const ColorThemeReader::MetaColorNode::Ptr metaTree);
    ColorTree(ColorTree&& other) noexcept;
    ColorTree& operator=(ColorTree&& other) noexcept;

    ColorTree() = delete;
    ColorTree(const ColorTree&) = delete;
    ColorTree& operator=(const ColorTree&) = delete;

public:
    /**
     * Colors which described on a root json level ("light3", "brand_core" etc).
     */
    QMap<QString, QColor> getRootColors() const;

    /**
     * Root colors which values differ in the trees.
     */
    ColorSubstitutions rootColorsDelta(const ColorTree& other) const;

    using ColorData = std::variant<QColor, QList<QColor>>;
    using ColorMap = QMap<QString, ColorData>;

    /**
     * Map from color path to a single color or a color list.
     */
    ColorMap colorsByPath() const;

    /**
     * Color tree as a variant map for use in QML.
     */
    QVariantMap toVariantMap() const;

    /**
     * Map from color group name to color values list,
     * map from color value to group name and color index.
     */
    void baseColorGroups(ColorGroups* groups, QHash<QColor, ColorInfo>* colorInfo) const;

private:
    void copyNodes(
        ColorTree::Node::Ptr parentNode,
        ColorThemeReader::MetaColorNode::Ptr metaNode) const;

    ColorGroups createBaseColorGroups() const;

    void getColorGroups(ColorGroups* result, const ColorTree::Node::Ptr parentNode) const;

private:
    Node::Ptr m_root;
};

} // namespace nx::vms::client::core
