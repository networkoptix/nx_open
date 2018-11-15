#ifndef SERVER_ADDRESSES_MODEL_H
#define SERVER_ADDRESSES_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>

#include <client/client_model_types.h>
#include <client/client_color_types.h>

#include <ui/customization/customized.h>
#include <nx/utils/url.h>

class QnServerAddressesModel : public Customized<QAbstractItemModel> {
    Q_OBJECT
    Q_PROPERTY(QnRoutingManagementColors colors READ colors WRITE setColors)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef Customized<QAbstractItemModel> base_type;

public:
    enum {
        AddressColumn = 0,
        InUseColumn,
        ColumnCount
    };

    enum EditingError {
        InvalidUrl,
        ExistingUrl
    };

    explicit QnServerAddressesModel(QObject *parent = 0);

    void setPort(int port);
    int port() const;

    void setAddressList(const QList<nx::utils::Url> &addresses);
    QList<nx::utils::Url> addressList() const;

    void setManualAddressList(const QList<nx::utils::Url> &addresses);
    QList<nx::utils::Url> manualAddressList() const;

    void setIgnoredAddresses(const QSet<nx::utils::Url> &ignoredAddresses);
    QSet<nx::utils::Url> ignoredAddresses() const;

    void addAddress(const nx::utils::Url &url, bool isManualAddress = true);
    void removeAddressAtIndex(const QModelIndex &index);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void clear();
    void resetModel(const QList<nx::utils::Url> &addresses, const QList<nx::utils::Url> &manualAddresses, const QSet<nx::utils::Url> &ignoredAddresses, int port);

    bool isManualAddress(const QModelIndex &index) const;

    // Reimplemented from QAbstractItemModel
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    const QnRoutingManagementColors colors() const;
    void setColors(const QnRoutingManagementColors &colors);

signals:
    void urlEditingFailed(const QModelIndex &index, int error);

private:
    nx::utils::Url addressAtIndex(const QModelIndex &index, int defaultPort = -1) const;

private:
    bool m_readOnly;
    int m_port;
    QList<nx::utils::Url> m_addresses;
    QList<nx::utils::Url> m_manualAddresses;
    QSet<nx::utils::Url> m_ignoredAddresses;

    QnRoutingManagementColors m_colors;
};

class QnSortedServerAddressesModel : public QSortFilterProxyModel {
public:
    explicit QnSortedServerAddressesModel(QObject *parent = 0);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};

#endif // SERVER_ADDRESSES_MODEL_H
