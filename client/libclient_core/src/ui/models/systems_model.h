#pragma once

#include <QtCore/QStringListModel>

#include <utils/common/connective.h>

class QnSystemsModelPrivate;

class QnSystemsModel : public Connective<QAbstractListModel>
{
    Q_OBJECT
    Q_PROPERTY(QString minimalVersion READ minimalVersion WRITE setMinimalVersion NOTIFY minimalVersionChanged)

    using base_type = Connective<QAbstractListModel>;

public:
    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1,

        SearchRoleId = FirstRoleId,
        SystemNameRoleId,
        SystemIdRoleId,

        OwnerDescriptionRoleId,
        LastPasswordRoleId,

        IsFactorySystemRoleId,

        IsCloudSystemRoleId,
        IsOnlineRoleId,
        IsCompatibleRoleId,
        IsCompatibleVersionRoleId,
        IsCompatibleInternalRoleId,

        WrongVersionRoleId,
        CompatibleVersionRoleId,

        // For local systems
        LastPasswordsModelRoleId,

        RolesCount
    };

public:
    QnSystemsModel(QObject* parent = nullptr);
    virtual ~QnSystemsModel();

    QString minimalVersion() const;
    void setMinimalVersion(const QString& minimalVersion);

public: // overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void minimalVersionChanged();

private:
    QScopedPointer<QnSystemsModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnSystemsModel)
};
