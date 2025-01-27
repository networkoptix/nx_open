// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QVariant>

class QQuickItem;

namespace nx::vms::client::core {

class NameValueTableCalculator: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit NameValueTableCalculator(QObject* parent = nullptr);

    Q_INVOKABLE void calculateWidths(
        QQuickItem* grid, const QVariantList& labels, const QVariantList& values);

    static void registerQmlType();

private:
    struct RowAsset
    {
        int index;
        qreal valueWidth;
    };

private:
    bool initialize(QQuickItem* grid, const QVariantList& labels, const QVariantList& values);
    void setWidths(const QVariantList& container, qreal newWidth);
    void reduceValuesCount(int index);

private:
    qreal m_columnSpacing;
    qreal m_leftPadding;
    qreal m_rightPadding;
    qreal m_availableWidth;
    qreal m_itemSpacing;
    qreal m_labelFraction;
    qreal m_maxLabelWidth;
    QVariantList m_labels;
    QVariantList m_values;
    QList<RowAsset> m_rowWidths;
    QList<int> m_valuesVisibleCount;
};

} // namespace nx::vms::client::core
