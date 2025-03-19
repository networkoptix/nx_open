// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "name_value_table_calculator.h"

#include <QtCore/QVariant>
#include <QtQuick/private/qquickrepeater_p.h>
#include <QtQuick/private/qquicktext_p.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/qml/items/values_text.h>
#include <nx/vms/client/core/utils/qml_property.h>

namespace nx::vms::client::core {

struct NameValueTableCalculator::Private
{
    struct RowAsset
    {
        int index;
        qreal valueWidth;
    };

    NameValueTableCalculator* const q;
    QmlProperty<qreal> columnSpacing;
    QmlProperty<qreal> leftPadding;
    QmlProperty<qreal> rightPadding;
    QmlProperty<int> maxRowCount;
    qreal availableWidth;
    QmlProperty<qreal> labelFraction;
    qreal maxLabelWidth;
    QList<QQuickText*> labels;
    QList<ValuesText*> values;
    QList<RowAsset> rowWidths;
    QQuickItem* grid = nullptr;

    int visibleRowCount() const
    {
        return maxRowCount.value() > 0
            ? qMin(maxRowCount.value(), labels.count())
            : labels.count();
    }

    void initValueWidths()
    {
        rowWidths.clear();
        rowWidths.reserve(values.count());
        for (int i = 0; i < values.count(); ++i)
            rowWidths.append({i, values[i]->implicitWidth()});
    }

    void initMaxLabelWidth()
    {
        maxLabelWidth = 0;
        for (int i = 0; i < visibleRowCount(); ++i)
            maxLabelWidth = qMax(maxLabelWidth, labels[i]->implicitWidth());
    }

    void updateAvailableWidth()
    {
        availableWidth = grid->width() -
            columnSpacing.value() -
            leftPadding.value() -
            rightPadding.value();
    }

    void updateChildren()
    {
        labels.clear();
        values.clear();
        if (!grid)
            return;

        for (const auto child: grid->childItems())
        {
            if (child->objectName() == "cellLabel")
            {
                if (auto label = qobject_cast<QQuickText*>(child))
                    labels.append(label);
            }
            else if (child->objectName() == "cellValues")
            {
                if (auto value = qobject_cast<ValuesText*>(child))
                    values.append(value);
            }
        }
        initMaxLabelWidth();
    }

    void sortWidths()
    {
        std::sort(rowWidths.begin(), rowWidths.end(), [](RowAsset a, RowAsset b)
        {
            return a.valueWidth > b.valueWidth;
        });
    }

    void setWidths(qreal labelsWidth, qreal valuesWidth)
    {
        std::for_each(labels.begin(), labels.end(),
            [labelsWidth](QQuickItem* item){ item->setWidth(labelsWidth); });
        std::for_each(values.begin(), values.end(),
            [valuesWidth](QQuickItem* item){ item->setWidth(valuesWidth); });
    }
};

NameValueTableCalculator::NameValueTableCalculator(QObject* parent):
    base_type(parent),
    d(new Private{.q = this})
{
}

NameValueTableCalculator::~NameValueTableCalculator()
{
}

void NameValueTableCalculator::updateColumnWidths()
{
    if (d->labels.isEmpty() || d->values.isEmpty() || d->labels.count() != d->values.count())
        return;

    d->initValueWidths();
    d->initMaxLabelWidth();
    qreal preferredLabelWidth = d->maxLabelWidth;
    qreal preferredValueWidth = d->availableWidth - preferredLabelWidth;

    d->sortWidths();
    if (d->maxLabelWidth + d->rowWidths.first().valueWidth <= d->availableWidth)
    {
        preferredValueWidth = d->availableWidth - preferredLabelWidth;
        d->setWidths(preferredLabelWidth, preferredValueWidth);
        return;
    }

    bool mayBeNarrowed = true;
    qreal effectiveWidth = 0;
    for (int i = 0; i < d->visibleRowCount(); ++i)
    {
        const int index = d->rowWidths[i].index;
        auto value = d->values[index];
        if (value->implicitWidth() > preferredValueWidth)
            mayBeNarrowed = false;
        effectiveWidth = qMax(effectiveWidth, value->implicitWidth());
    }
    if (mayBeNarrowed)
    {
        d->setWidths(preferredLabelWidth, preferredValueWidth);
        return;
    }

    if (d->availableWidth * (1 - d->labelFraction) >= effectiveWidth)
    {
        preferredValueWidth = effectiveWidth;
        preferredLabelWidth = d->availableWidth - preferredValueWidth;
        d->setWidths(preferredLabelWidth, preferredValueWidth);
        return;
    }

    preferredLabelWidth = qMin(d->maxLabelWidth, d->availableWidth * d->labelFraction);
    preferredValueWidth = d->availableWidth - preferredLabelWidth;
    effectiveWidth = 0;
    for (int i = 0; i < d->visibleRowCount(); ++i)
    {
        auto value = d->values[i];
        value->setWidth(preferredValueWidth);
        effectiveWidth = qMax(effectiveWidth, value->effectiveWidth());
    }
    if (effectiveWidth < preferredValueWidth)
    {
        preferredValueWidth = effectiveWidth;
        preferredLabelWidth = d->availableWidth - preferredValueWidth;
    }
    d->setWidths(preferredLabelWidth, preferredValueWidth);
}

void NameValueTableCalculator::onContextReady()
{
    if (auto grid = qobject_cast<QQuickItem*>(parent()))
    {
        d->grid = grid;
        connect(d->grid, &QQuickItem::childrenChanged, this,
            [this]()
            {
                d->updateChildren();
                updateColumnWidths();
            });

        auto updateWidths = [this]()
            {
                d->updateAvailableWidth();
                updateColumnWidths();
            };

        d->columnSpacing = QmlProperty<qreal>(d->grid, "columnSpacing");
        d->columnSpacing.connectNotifySignal(this, updateWidths);

        d->leftPadding = QmlProperty<qreal>(d->grid, "leftPadding");
        d->leftPadding.connectNotifySignal(this, updateWidths);

        d->rightPadding = QmlProperty<qreal>(d->grid, "rightPadding");
        d->rightPadding.connectNotifySignal(this, updateWidths);

        d->labelFraction = QmlProperty<qreal>(d->grid->parentItem(), "labelFraction");
        d->labelFraction.connectNotifySignal(this, &NameValueTableCalculator::updateColumnWidths);

        d->maxRowCount = QmlProperty<int>(d->grid, "maxRowCount");
        d->maxRowCount.connectNotifySignal(this,&NameValueTableCalculator::updateColumnWidths);

        connect(d->grid, &QQuickItem::widthChanged, this, updateWidths);

        d->updateChildren();
        updateWidths();
    }
}

void NameValueTableCalculator::registerQmlType()
{
    qmlRegisterType<NameValueTableCalculator>("Nx.Core", 1, 0, "NameValueTableCalculator");
}

} // namespace nx::vms::client::core
