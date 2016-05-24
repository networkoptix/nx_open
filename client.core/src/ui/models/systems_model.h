#pragma once

#include <QtCore/QStringListModel>

#include <utils/common/connective.h>
#include <network/system_description.h>
#include <nx/utils/disconnect_helper.h>

class QnSystemsModel : public Connective<QAbstractListModel>
{
    Q_OBJECT
    typedef Connective<QAbstractListModel> base_type;

public:
    QnSystemsModel(QObject *parent = nullptr);

    virtual ~QnSystemsModel();

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    void updateOwnerDescription();

    void addSystem(const QnSystemDescriptionPtr &systemDescription);

    void removeSystem(const QString &systemId);

    struct InternalSystemData;
    typedef QSharedPointer<InternalSystemData> InternalSystemDataPtr;
    typedef QVector<InternalSystemDataPtr> InternalList;

    InternalList::iterator getInternalDataIt(
        const QnSystemDescriptionPtr &systemDescription);

    void serverChanged(const QnSystemDescriptionPtr &systemDescription
        , const QnUuid &serverId
        , QnServerFields fields);

    QStringListModel *createStringListModel(const QStringList &data) const;

private:
    typedef std::function<bool(const InternalSystemDataPtr &first
        , const InternalSystemDataPtr &second)> LessPred;

    const int m_maxCount;
    const LessPred m_lessPred;
    QnDisconnectHelper m_connections;
    InternalList m_internalData;
};
