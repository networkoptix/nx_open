
#pragma once

#include <QtCore/QAbstractListModel>

#include <network/system_description.h>

class QnSystemsModel : public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    QnSystemsModel(QObject *parent = nullptr);

    virtual ~QnSystemsModel();

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const;

private:
    void addSystem(const QnSystemDescriptionPtr &systemDescription);

    void removeSystem(const QnUuid &systemId);

private:
    typedef QVector<QnSystemDescriptionPtr> SystemsList;

    SystemsList m_systems;
};