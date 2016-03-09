
#pragma once

#include <QtCore/QAbstractListModel>

#include <utils/common/connective.h>
#include <network/system_description.h>

class QnConnectionsHolder;

class QnSystemsModel : public Connective<QAbstractListModel>
{
    Q_OBJECT
    typedef Connective<QAbstractListModel> base_type;

public:
    QnSystemsModel(int maxCount
        , QObject *parent = nullptr);

    virtual ~QnSystemsModel();

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const;

private:
    void addSystem(const QnSystemDescriptionPtr &systemDescription);

    void removeSystem(const QnUuid &systemId);

    struct InternalSystemData;
    typedef QSharedPointer<InternalSystemData> InternalSystemDataPtr;
    typedef QVector<InternalSystemDataPtr> InternalList;

    InternalList::iterator getInternalDataIt(
        const QnSystemDescriptionPtr &systemDescription);

    void serverChanged(const QnSystemDescriptionPtr &systemDescription
        , const QnUuid &serverId
        , QnServerFields fields);

private:
    typedef QScopedPointer<QnConnectionsHolder> QnConnectionsHolderPtr;
    typedef std::function<bool(const InternalSystemDataPtr &first
        , const InternalSystemDataPtr &second)> LessPred;

    const int m_maxCount;
    const QnConnectionsHolderPtr m_connectionsHolder;
    const LessPred m_lessPred;
    InternalList m_internalData;
};