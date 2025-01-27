// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/private/qquickrepeater_p.h>

#include <nx/vms/client/core/utils/qml_property.h>

#include "name_value_table_calculator.h"

namespace nx::vms::client::core {

NameValueTableCalculator::NameValueTableCalculator(QObject* parent):
    base_type(parent)
{
}

void NameValueTableCalculator::calculateWidths(
    QQuickItem* grid, const QVariantList& labels, const QVariantList& values)
{
    if (!initialize(grid, std::move(labels), std::move(values)))
        return;

    qreal preferredLabelWidth = m_availableWidth * m_labelFraction;
    qreal preferredValueWidth = m_availableWidth - preferredLabelWidth;

    // Iterative processing.
    bool found = false;
    do
    {
        std::sort(m_rowWidths.begin(), m_rowWidths.end(), [](RowAsset a, RowAsset b)
            {
                return a.valueWidth > b.valueWidth;
            });

        qreal excessWidth = m_maxLabelWidth + m_rowWidths.first().valueWidth - m_availableWidth;
        if (excessWidth <= 0)
            return;

        if (m_maxLabelWidth - excessWidth >= preferredLabelWidth)
        {
            // It is enough to shrink the label column only.
            if (preferredValueWidth > m_rowWidths[0].valueWidth)
            {
                preferredValueWidth = m_rowWidths[0].valueWidth;
                preferredLabelWidth = m_availableWidth - preferredValueWidth;
            }
            found = true;
            break;
        }

        for (int i = 0; i < m_rowWidths.count(); ++i)
        {
            const int index = m_rowWidths[i].index;
            if (m_rowWidths[i].valueWidth < preferredValueWidth)
            {
                found = true;
                break;
            }

            if (m_valuesVisibleCount[index] == 1)
            {
                if (m_rowWidths[i].valueWidth > preferredValueWidth)
                    m_rowWidths[i].valueWidth = preferredValueWidth;
            }
            else if (m_rowWidths[i].valueWidth > preferredValueWidth)
            {
                reduceValuesCount(i);
                break;
            }
        }
    } while (!found);

    setWidths(labels, preferredLabelWidth);
    setWidths(values, preferredValueWidth);
}

bool NameValueTableCalculator::initialize(
    QQuickItem* grid, const QVariantList& labels, const QVariantList& values)
{
    m_labels = std::move(labels);
    m_values = std::move(values);
    if (!grid
        || grid->width() <= 8
        || m_labels.count() == 0
        || m_values.count() == 0
        || m_labels.count() != m_values.count())
        return false;

    m_columnSpacing = QmlProperty<qreal>(grid, "columnSpacing").value();
    m_leftPadding = QmlProperty<qreal>(grid, "leftPadding").value();
    m_rightPadding = QmlProperty<qreal>(grid, "rightPadding").value();
    m_availableWidth = grid->width() - m_columnSpacing - m_leftPadding - m_rightPadding;
    m_labelFraction = QmlProperty<qreal>(grid->parentItem(), "labelFraction");

    m_rowWidths.clear();
    m_rowWidths.reserve(m_values.count());
    m_valuesVisibleCount.clear();
    m_valuesVisibleCount.reserve(m_values.count());
    m_maxLabelWidth = 0;
    for (int i = 0; i < m_values.count(); ++i)
    {
        auto labelItem = m_labels[i].value<QQuickItem*>();
        labelItem->resetWidth();
        labelItem->setWidth(labelItem->implicitWidth());
        m_maxLabelWidth = qMax(m_maxLabelWidth, labelItem->width());

        auto valueItem = m_values[i].value<QQuickItem*>();
        m_itemSpacing = QmlProperty<qreal>(valueItem, "spacing").value();
        const auto valueRowRepeater = valueItem->findChild<QQuickRepeater*>("valueRowRepeater");

        const auto rowValuesCount = valueRowRepeater->count();
        valueItem->setProperty("lastVisibleIndex", rowValuesCount - 1);
        m_valuesVisibleCount.append(rowValuesCount);
        qreal valueWidth = 0;
        for (int j = 0; j < rowValuesCount; ++j)
        {
            if (valueWidth > 0)
                valueWidth += m_itemSpacing;

            auto valueSubItem = valueRowRepeater->itemAt(j);
            valueSubItem->resetWidth();
            valueWidth += valueSubItem->implicitWidth();
        }
        valueItem->resetWidth();
        m_rowWidths.append({i, valueWidth});
    }

    return true;
}

void NameValueTableCalculator::setWidths(const QVariantList& container, qreal newWidth)
{
    std::for_each(container.begin(), container.end(),
        [newWidth](QVariant item){ item.value<QQuickItem*>()->setWidth(newWidth); });
}

void NameValueTableCalculator::reduceValuesCount(int index)
{
    auto& rowWidthElement = m_rowWidths[index];
    auto valueItem = m_values[rowWidthElement.index].value<QQuickItem*>();
    valueItem->setProperty(
        "lastVisibleIndex", --m_valuesVisibleCount[rowWidthElement.index] - 1);
    const auto valueRowRepeater =
        valueItem->findChild<QQuickRepeater*>("valueRowRepeater");
    const auto appendix = valueItem->findChild<QQuickItem*>("appendix");
    qreal valueWidth = appendix->implicitWidth();
    for (int j = 0; j < m_valuesVisibleCount[rowWidthElement.index]; ++j)
    {
        valueWidth += m_itemSpacing;

        auto valueSubItem = valueRowRepeater->itemAt(j);
        valueWidth += valueSubItem->implicitWidth();
    }
    rowWidthElement.valueWidth = valueWidth;
}

void NameValueTableCalculator::registerQmlType()
{
    qmlRegisterSingletonType<NameValueTableCalculator>(
        "Nx.Core", 1, 0, "NameValueTableCalculator",
        [](QQmlEngine*, QJSEngine*) { return new NameValueTableCalculator(); });
}

} // namespace nx::vms::client::core
