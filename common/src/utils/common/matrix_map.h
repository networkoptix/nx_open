#ifndef QN_MATRIX_MAP_H
#define QN_MATRIX_MAP_H

#include <cassert>
#include <QPoint>
#include <QRect>
#include <QHash>
#include <QSet>
#include <QPair>

inline uint qHash(const QPoint &value) {
    using ::qHash;

    return qHash(qMakePair(value.x(), value.y()));
}

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
        foreach(const QRect &region, regions)
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
     * \param emptyAllowed              Whether unoccupied positions are allowed.
     * \returns                         Whether the given region is occupied by given value only.
     */
    bool isOccupiedBy(const QRect &region, const T &value, bool emptyAllowed) const {
        for (int r = region.top(); r <= region.bottom(); r++) {
            for (int c = region.left(); c <= region.right(); c++) {
                typename QHash<QPoint, T>::const_iterator pos = m_itemByPosition.find(QPoint(c, r));
                if(pos == m_itemByPosition.end()) {
                    if(!emptyAllowed)
                        return false;
                } else {
                    if(*pos != value)
                        return false;
                }
            }
        }

        return true;
    }

private:
    QHash<QPoint, T> m_itemByPosition;
};

#endif // QN_MATRIX_MAP_H
