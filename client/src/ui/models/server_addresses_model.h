#ifndef SERVER_ADDRESSES_MODEL_H
#define SERVER_ADDRESSES_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>

#include <client/client_model_types.h>
#include <client/client_color_types.h>

#include <ui/customization/customized.h>

class QnServerAddressesModel : public Customized<QAbstractItemModel> {
    Q_OBJECT
    Q_PROPERTY(QnRoutingManagementColors colors READ colors WRITE setColors)
    typedef Customized<QAbstractItemModel> base_type;

public:
    enum {
        AddressColumn = 0,
        InUseColumn,
        ColumnCount
    };

    explicit QnServerAddressesModel(QObject *parent = 0);

    void setAddressList(const QList<QUrl> &addresses);
    QList<QUrl> addressList() const;

    void setManualAddressList(const QList<QUrl> &addresses);
    QList<QUrl> manualAddressList() const;

    void setIgnoredAddresses(const QSet<QUrl> &ignoredAddresses);
    QSet<QUrl> ignoredAddresses() const;

    void clear();
    void resetModel(const QList<QUrl> &addresses, const QList<QUrl> &manualAddresses, const QSet<QUrl> &ignoredAddresses);

    bool isManualAddress(const QModelIndex &index) const;

    // Reimplemented from QAbstractItemModel
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    const QnRoutingManagementColors colors() const;
    void setColors(const QnRoutingManagementColors &colors);

signals:
    void ignoreChangeRequested(const QString &addressAtIndex, bool ignore);

private:
    QUrl addressAtIndex(const QModelIndex &index) const;

private:
    QList<QUrl> m_addresses;
    QList<QUrl> m_manualAddresses;
    QSet<QUrl> m_ignoredAddresses;

    QnRoutingManagementColors m_colors;
};

class QnSortedServerAddressesModel : public QSortFilterProxyModel {
public:
    explicit QnSortedServerAddressesModel(QObject *parent = 0);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};

#endif // SERVER_ADDRESSES_MODEL_H
