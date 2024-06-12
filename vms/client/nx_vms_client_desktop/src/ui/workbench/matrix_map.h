// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QSet>

#include <nx/utils/log/assert.h>
#include <utils/common/hash.h>

/**
 * Matrix map is an abstraction of an infinite two-dimensional sparse array.
 *
 * It presents some useful functions for operating on rectangular regions.
 */
template<class T>
class QnMatrixMap
{
public:
    /**
     * \param region                    Region to clear.
     */
    void clear(const QRect& region)
    {
        for (int row = region.top(); row <= region.bottom(); ++row)
        {
            for (int col = region.left(); col <= region.right(); ++col)
                m_itemByPosition.remove(QPoint(col, row));
        }
    }

    /**
     * \param region Region to fill.
     * \param value Value to fill with.
     */
    void fill(const QRect& region, const T& value)
    {
        for (int row = region.top(); row <= region.bottom(); ++row)
        {
            for (int col = region.left(); col <= region.right(); ++col)
                m_itemByPosition[QPoint(col, row)] = value;
        }
    }

    /**
     * \param position Position to get value at.
     * \param defaultValue Value to return in case the given position is not occupied.
     * \returns Value at the given position.
     */
    T value(const QPoint& position, const T& defaultValue = T()) const
    {
        const auto pos = m_itemByPosition.find(position);
        return pos != m_itemByPosition.end() ? *pos : defaultValue;
    }

    /**
     * \param region Region to examine for values.
     * \param[out] items Set to add found values to.
     */
    void values(const QRect& region, QSet<T>* items) const
    {
        NX_ASSERT(items != nullptr);

        for (int row = region.top(); row <= region.bottom(); ++row)
        {
            for (int col = region.left(); col <= region.right(); ++col)
            {
                auto pos = m_itemByPosition.find(QPoint(col, row));
                if (pos != m_itemByPosition.end())
                    items->insert(*pos);
            }
        }
    }

    /**
     * \param region Region to examine for values.
     * \returns Set of values that the given region contains.
     */
    QSet<T> values(const QRect& region) const
    {
        QSet<T> result;
        values(region, &result);
        return result;
    }

    /**
     * \param regions Regions to examine for values.
     * \returns Set of values that the given regions contain.
     */
    QSet<T> values(const QList<QRect>& regions) const
    {
        QSet<T> result;
        for (const QRect& region: regions)
            values(region, &result);
        return result;
    }

    /**
     * \param position Position to examine.
     * \returns Whether the given position is occupied.
     */
    bool isOccupied(const QPoint& position) const
    {
        return m_itemByPosition.find(position) != m_itemByPosition.end();
    }

    /**
     * \param region Region to examine.
     * \returns Whether there exists an occupied position in the given region.
     */
    bool isOccupied(const QRect& region) const
    {
        for (int row = region.top(); row <= region.bottom(); ++row)
        {
            for (int col = region.left(); col <= region.right(); ++col)
            {
                if (isOccupied(QPoint(col, row)))
                    return true;
            }
        }
        return false;
    }

    /**
     * \param region Region to examine.
     * \param value Value to check.
     * \param[out] conforming List to add conforming coordinates to.
     * \param[out] nonconforming List to add non-conforming coordinates to.
     * \returns Whether the given region is occupied by given value only. Note that empty space is
     * allowed.
     */
    bool isOccupiedBy(
        const QRect& region,
        const T& value,
        QSet<QPoint>* conforming = nullptr,
        QSet<QPoint>* nonconforming = nullptr) const
    {
        return isOccupiedBy(
            region, SingleValueConformanceChecker(value), conforming, nonconforming);
    }

    bool isOccupiedBy(
        const QRect& region,
        const QSet<T>& values,
        QSet<QPoint>* conforming = nullptr,
        QSet<QPoint>* nonconforming = nullptr) const
    {
        return isOccupiedBy(region, ValueSetConformanceChecker(values), conforming, nonconforming);
    }

    QList<QPoint> positions() const
    {
        return m_itemByPosition.keys();
    }

protected:
    struct SingleValueConformanceChecker
    {
    public:
        SingleValueConformanceChecker(const T& value): m_value(value) {}

        bool operator()(const T& value) const { return value == m_value; }

    private:
        const T& m_value;
    };

    struct ValueSetConformanceChecker
    {
    public:
        ValueSetConformanceChecker(const QSet<T>& values): m_values(values) {}

        bool operator()(const T& value) const { return m_values.contains(value); }

    private:
        const QSet<T>& m_values;
    };

    template<class ConformanceChecker>
    bool isOccupiedBy(
        const QRect& region,
        const ConformanceChecker& checker,
        QSet<QPoint>* conforming = nullptr,
        QSet<QPoint>* nonconforming = nullptr) const
    {
        if (conforming == nullptr && nonconforming == nullptr)
        {
            return isOccupiedBy<true>(region, checker, nullptr, nullptr);
        }

        if (conforming != nullptr && nonconforming != nullptr)
        {
            return isOccupiedBy<false>(region, checker, conforming, nonconforming);
        }

        NX_ASSERT(false,
            "'conforming' and 'nonconforming' parameters must either be both nullptr or both non-nullptr.");
        return isOccupiedBy<true>(region, checker, nullptr, nullptr);
    }

    template<bool returnEarly, class ConformanceChecker>
    bool isOccupiedBy(
        const QRect& region,
        const ConformanceChecker& checker,
        QSet<QPoint>* conforming,
        QSet<QPoint>* nonconforming) const
    {
        for (int row = region.top(); row <= region.bottom(); ++row)
        {
            for (int col = region.left(); col <= region.right(); ++col)
            {
                bool conforms = true;
                const QPoint point(col, row);

                const auto pos = m_itemByPosition.find(point);
                if (pos != m_itemByPosition.end())
                    conforms = checker(*pos);

                if (returnEarly)
                {
                    if (!conforms)
                        return false;
                }
                else
                {
                    if (conforms)
                        conforming->insert(point);
                    else
                        nonconforming->insert(point);
                }
            }
        }
        return true;
    }

private:
    QHash<QPoint, T> m_itemByPosition;
};
