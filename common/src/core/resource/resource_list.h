#ifndef QN_RESOURCE_LIST_H
#define QN_RESOURCE_LIST_H

#include <QList>
#include <QtAlgorithms>

template<class T>
class QnList: public QList<T> {
    typedef QList<T> base_type;

public:
    QnList() {}

    QnList(const QList<T> &other): base_type(other) {}

    template<class Y>
    explicit QnList(const QList<Y> &other) {
        reserve(other.size());
        qCopy(other.begin(), other.end(), begin());
    }
};


#endif // QN_RESOURCE_LIST_H
