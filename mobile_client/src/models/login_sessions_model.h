#pragma once

#include <QtCore/QAbstractListModel>

struct QnLoginSession;

class QnLoginSessionsModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        SystemNameRole = Qt::UserRole + 1,
        AddressRole,
        PortRole,
        UserRole,
        PasswordRole,
        SessionIdRole,
        SessionRoleCount
    };

    QnLoginSessionsModel(QObject *parent = nullptr);
    ~QnLoginSessionsModel();

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    virtual const QnLoginSession *session(int row) const = 0;
};
