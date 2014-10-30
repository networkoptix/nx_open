#ifndef QN_MATRIX_MAP_H
#define QN_MATRIX_MAP_H

#include <cassert>

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QHash>
#include <QtCore/QSet>
#include <QtCore/QPair>

#include <utils/common/warnings.h>
#include <utils/common/hash.h>
#include <utils/common/collection.h>

/**
 * Matrix map is an abstraction of an infinite two-dimensional sparse array.
 * 
 * It presents some useful functions for operating on rectangular regions.
 */
template<class T>
class QnMatrixMap {
public:
    /**
     * \param region                    Region to clear.
     */
    void clear(const QRect &region) {
        for (int r = region.top(); r <= region.bottom(); r++)
            for (int c = region.left(); c <= region.right(); c++)
                m_itemByPosition.remove(QPoint(c, r));
    }

    /**
     * \param region                    Region to fill.
     * \param value                     Value to fill with.
     */
    void fill(const QRect &region, const T &value) {
        for (int r = region.top(); r <= region.bottom(); r++) 
            for (int c = region.left(); c <= region.right(); c++) 
                m_itemByPosition[QPoint(c, r)] = value;
    }

    /**
     * \param position                  Position to get value at.
     * \param defaultValue              Value to return in case the given position is not occupied.
     * \returns                         Value at the given position.
     */
    T value(const QPoint &position, const T &defaultValue = T()) const {
        typename QHash<QPoint, T>::const_iterator pos = m_itemByPosition.find(position);
        
        return pos != m_itemByPosition.end() ? *pos : defaultValue;
    }

    /**
     * \param region                    Region to examine for values.
     * \param[out] items                Set to add found values to.
     */
    void values(const QRect &region, QSet<T> *items) const {
        assert(items != NULL);

        for (int r = region.top(); r <= region.bottom(); ++r) {
            for (int c = region.left(); c <= region.right(); ++c) {
                typename QHash<QPoint, T>::const_iterator pos = m_itemByPosition.find(QPoint(c, r));
                if (pos != m_itemByPosition.end())
                    items->insert(*pos);
            }
        }
    }

    /**
     * \param region                    Region to examine for values.
     * \returns                         Set of values that the given region contains.
     */
    QSet<T> values(const QRect &region) const {
        QSet<T> result;
        values(region, &result);
        return result;
    }

    /**
     * \param regions                   Regions to examine for values.
     * \returns                         Set of values that the given regions contain.
     */
    QSet<T> values(const QList<QRect> &regions) const {
        QSet<T> result;
        for(const QRect &region: regions)
            values(region, &result);
        return result;
    }

    /**
     * \param position                  Position to examine.
     * \returns                         Whether the given position is occupied.
     */
    bool isOccupied(const QPoint &position) const {
        return m_itemByPosition.find(position) != m_itemByPosition.end();
    }

    /**
     * \param region                    Region to examine.
     * \returns                         Whether there exists an occupied position in the given region.
     */
    bool isOccupied(const QRect &region) const {
        for (int r = region.top(); r <= region.bottom(); r++)
            for (int c = region.left(); c <= region.right(); c++)
                if (isOccupied(QPoint(c, r)))
                    return true;

      return false;
    }

    /**
     * \param region                    Region to examine.
     * \param value                     Value to check.
     * \param[out] conforming           List to add conforming coordinates to.
     * \param[out] nonconforming        List to add non-conforming coordinates to.
     * \returns                         Whether the given region is occupied by given value only. Note that empty space is allowed.
     */
    bool isOccupiedBy(const QRect &region, const T &value, QList<QPoint> *conforming = NULL, QList<QPoint> *nonconforming = NULL) const {
        return isOccupiedBy(region, SingleValueConformanceChecker(value), conforming, nonconforming);
    }

    bool isOccupiedBy(const QRect &region, const T &value, QSet<QPoint> *conforming = NULL, QSet<QPoint> *nonconforming = NULL) const {
        return isOccupiedBy(region, SingleValueConformanceChecker(value), conforming, nonconforming);
    }

    bool isOccupiedBy(const QRect &region, const QSet<T> &values, QList<QPoint> *conforming = NULL, QList<QPoint> *nonconforming = NULL) const {
        return isOccupiedBy(region, ValueSetConformanceChecker(values), conforming, nonconforming);
    }

    bool isOccupiedBy(const QRect &region, const QSet<T> &values, QSet<QPoint> *conforming = NULL, QSet<QPoint> *nonconforming = NULL) const {
        return isOccupiedBy(region, ValueSetConformanceChecker(values), conforming, nonconforming);
    }

protected:
    struct SingleValueConformanceChecker {
    public:
        SingleValueConformanceChecker(const T &value): m_value(value) {}

        bool operator()(const T &value) const {
            return value == m_value;
        }

    private:
        const T &m_value;
    };

    struct ValueSetConformanceChecker {
    public:
        ValueSetConformanceChecker(const QSet<T> &values): m_values(values) {}

        bool operator()(const T &value) const {
            return m_values.contains(value);
        }

    private:
        const QSet<T> &m_values;
    };

    template<class ConformanceChecker, class PointContainer>
    bool isOccupiedBy(const QRect &region, const ConformanceChecker &checker, PointContainer *conforming = NULL, PointContainer *nonconforming = NULL) const {
        if(conforming == NULL && nonconforming == NULL) {
            return isOccupiedBy<true, PointContainer>(region, checker, NULL, NULL);
        } else if(conforming != NULL && nonconforming != NULL) {
            return isOccupiedBy<false, PointContainer>(region, checker, conforming, nonconforming);
        } else {
            qnWarning("'conforming' and 'nonconforming' parameters must either be both NULL or both non-NULL.");
            return isOccupiedBy<true, PointContainer>(region, checker, NULL, NULL);
        }
    }

    template<bool returnEarly, class PointContainer, class ConformanceChecker>
    bool isOccupiedBy(const QRect &region, const ConformanceChecker &checker, PointContainer *conforming, PointContainer *nonconforming) const {
        for (int r = region.top(); r <= region.bottom(); r++) {
            for (int c = region.left(); c <= region.right(); c++) {
                bool conforms = true;

                typename QHash<QPoint, T>::const_iterator pos = m_itemByPosition.find(QPoint(c, r));
                if(pos != m_itemByPosition.end())
                    conforms = checker(*pos);

                if(returnEarly) {
                    if(!conforms)
                        return false;
                } else {
                    if(conforms) {
                        QnCollection::insert(*conforming, conforming->end(), QPoint(c, r));
                    } else {
                        QnCollection::insert(*nonconforming, nonconforming->end(), QPoint(c, r));
                    }
                }
            }
        }

        return true;
    }



private:
    QHash<QPoint, T> m_itemByPosition;
};

#endif // QN_MATRIX_MAP_H
