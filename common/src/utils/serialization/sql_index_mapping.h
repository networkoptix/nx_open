#ifndef QN_SQL_INDEX_MAPPING_H
#define QN_SQL_INDEX_MAPPING_H

#include <cassert>

#include <QtCore/QVector>

namespace QnSqlDetail {
    class MappingVisitor;
    class FetchVisitor;
}


class QnSqlIndexMapping {
public:
    QnSqlIndexMapping() {}

    int index(int field) {
        assert(field >= 0);

        if(field >= indices.size())
            return -1;

        return indices[field];
    }

    void setIndex(int field, int index) {
        assert(field >= 0);
        
        if(field >= indices.size()) {
            indices.reserve(field + 1);
            for(int i = indices.size(); i <= field; i++)
                indices.push_back(-1);
        }

        indices[field] = index;
    }

private:
    friend class QnSqlDetail::MappingVisitor;
    friend class QnSqlDetail::FetchVisitor;

    QVector<int> indices;
};

#endif // QN_SQL_INDEX_MAPPING_H
