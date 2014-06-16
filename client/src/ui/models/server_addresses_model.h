#ifndef SERVER_ADDRESSES_MODEL_H
#define SERVER_ADDRESSES_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>

class QnServerAddressesModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum {
        AddressColumn = 0,
        IgnoredColumn,
        ColumnCount
    };

    explicit QnServerAddressesModel(QObject *parent = 0);

    void setAddressList(const QStringList &addresses);
    QStringList addressList() const;

    void setManualAddressList(const QStringList &addresses);
    QStringList manualAddressList() const;

    void setIgnoredAddresses(const QSet<QString> &ignoredAddresses);
    QSet<QString> ignoredAddresses() const;

    void resetModel(const QStringList &addresses, const QStringList &manualAddresses, const QSet<QString> &ignoredAddresses);

    bool isManualAddress(const QModelIndex &index) const;

    // Reimplemented from QAbstractItemModel
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QStringList m_addresses;
    QStringList m_manualAddresses;
    QSet<QString> m_ignoredAddresses;
};

class QnSortedServerAddressesModel : public QSortFilterProxyModel {
public:
    explicit QnSortedServerAddressesModel(QObject *parent = 0);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};

#endif // SERVER_ADDRESSES_MODEL_H
