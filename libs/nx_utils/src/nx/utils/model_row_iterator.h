// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QPointer>

namespace nx::utils {

class NX_UTILS_API ModelRowIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = QVariant;
    using pointer = const QVariant*;
    using reference = const QVariant&;

    static ModelRowIterator cbegin(
        int role,
        const QAbstractItemModel* model,
        int column = 0,
        const QModelIndex& parent = QModelIndex());

    static ModelRowIterator cend(
        int role,
        const QAbstractItemModel* model,
        int column = 0,
        const QModelIndex& parent = QModelIndex());

public:
    ModelRowIterator(int role, const QAbstractItemModel* model, int row, int column = 0,
        const QModelIndex& parent = QModelIndex());

    ModelRowIterator(const ModelRowIterator& other) = default;
    ~ModelRowIterator() = default;

    ModelRowIterator& operator=(const ModelRowIterator& other) = default;

    QVariant operator*() const { return index().data(m_role); }
    QVariant operator[](difference_type delta) const { return *(*this + delta); }

    ModelRowIterator operator+(difference_type delta) const { return sibling(m_row + delta); }
    ModelRowIterator operator-(difference_type delta) const { return operator+(-delta); }
    ModelRowIterator& operator+=(difference_type delta) { m_row += delta; return *this; }
    ModelRowIterator& operator-=(difference_type delta) { return operator+=(-delta); }

    difference_type operator-(const ModelRowIterator& other) const;

    ModelRowIterator& operator++() { ++m_row; return *this; }
    ModelRowIterator operator++(int);
    ModelRowIterator& operator--() { --m_row; return *this; }
    ModelRowIterator operator--(int);

    bool operator==(const ModelRowIterator& other) const;
    bool operator!=(const ModelRowIterator& other) const { return !operator==(other); }

    int row() const { return m_row; }
    QModelIndex index() const;

    friend ModelRowIterator operator+(difference_type delta, const ModelRowIterator& other)
    {
        return other + delta;
    }

private:
    ModelRowIterator sibling(int row) const;
    bool isSibling(const ModelRowIterator& other) const;

private:
    int m_role = Qt::DisplayRole;
    QPointer<const QAbstractItemModel> m_model;
    QPersistentModelIndex m_parent;
    int m_column = 0;
    int m_row = 0;
};

} // namespace nx:utils
