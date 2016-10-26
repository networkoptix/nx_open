
#pragma once

#include <QtCore/QSortFilterProxyModel>

class QnFilteringSystemsModel : public QSortFilterProxyModel
{
    Q_OBJECT
    typedef QSortFilterProxyModel base_type;

    Q_PROPERTY(int sourceRowsCount READ sourceRowsCount NOTIFY sourceRowsCountChanged)

public:
    QnFilteringSystemsModel(QObject* parent = nullptr);

    virtual ~QnFilteringSystemsModel() = default;


public: // properties
    int sourceRowsCount() const;

signals:
    void sourceRowsCountChanged();

private:
    void setSourceRowsCount(int value);

    void updateSourceRowsCount();

private:
    int m_sourceRowsCount;
};