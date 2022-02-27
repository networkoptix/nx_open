// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_addresses_model.h"

#include <QtCore/QSet>

#include "utils/common/util.h"

#include <nx/network/http/http_types.h>

#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include "nx/utils/string.h"

using namespace nx::vms::client::desktop;

QnServerAddressesModel::QnServerAddressesModel(QObject *parent)
    : base_type(parent)
    , m_readOnly(false)
    , m_port(-1)
{
}

void QnServerAddressesModel::setPort(int port) {
    beginResetModel();
    m_port = port;
    endResetModel();
}

int QnServerAddressesModel::port() const {
    return m_port;
}

void QnServerAddressesModel::setAddressList(const QList<nx::utils::Url> &addresses) {
    beginResetModel();
    m_addresses = addresses;
    endResetModel();
}

QList<nx::utils::Url> QnServerAddressesModel::addressList() const {
    return m_addresses;
}

void QnServerAddressesModel::setManualAddressList(const QList<nx::utils::Url> &addresses) {
    beginResetModel();
    m_manualAddresses = addresses;
    endResetModel();
}

QList<nx::utils::Url> QnServerAddressesModel::manualAddressList() const {
    return m_manualAddresses;
}

void QnServerAddressesModel::setIgnoredAddresses(const QSet<nx::utils::Url> &ignoredAddresses) {
    beginResetModel();
    m_ignoredAddresses = ignoredAddresses;
    endResetModel();
}

QSet<nx::utils::Url> QnServerAddressesModel::ignoredAddresses() const {
    return m_ignoredAddresses;
}

void QnServerAddressesModel::addAddress(const nx::utils::Url &url, bool isManualAddress) {
    int row;
    if (isManualAddress)
        row = m_addresses.size() + m_manualAddresses.size();
    else
        row = m_addresses.size();

    beginInsertRows(QModelIndex(), row, row);
    if (isManualAddress)
        m_manualAddresses.append(url);
    else
        m_addresses.append(url);
    endInsertRows();
}

void QnServerAddressesModel::removeAddressAtIndex(const QModelIndex &index) {
    if (!hasIndex(index.row(), index.column()))
        return;

    beginRemoveRows(QModelIndex(), index.row(), index.row());

    if (index.row() < m_addresses.size())
        m_addresses.removeAt(index.row());
    else
        m_manualAddresses.removeAt(index.row() - m_addresses.size());

    endRemoveRows();
}

bool QnServerAddressesModel::isReadOnly() const {
    return m_readOnly;
}

void QnServerAddressesModel::setReadOnly(bool readOnly) {
    if (m_readOnly == readOnly)
        return;

    beginResetModel();
    m_readOnly = readOnly;
    endResetModel();
}


void QnServerAddressesModel::resetModel(const QList<nx::utils::Url> &addresses, const QList<nx::utils::Url> &manualAddresses, const QSet<nx::utils::Url> &ignoredAddresses, int port) {
    beginResetModel();
    m_addresses = addresses;
    m_manualAddresses = manualAddresses;
    m_ignoredAddresses = ignoredAddresses;
    m_port = port;
    endResetModel();
}

void QnServerAddressesModel::clear() {
    beginResetModel();
    m_addresses.clear();
    m_manualAddresses.clear();
    m_ignoredAddresses.clear();
    endResetModel();
}

bool QnServerAddressesModel::isManualAddress(const QModelIndex &index) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return false;

    return index.row() >= m_addresses.size();
}

int QnServerAddressesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_addresses.size() + m_manualAddresses.size();
}

int QnServerAddressesModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    return ColumnCount;
}

QVariant QnServerAddressesModel::data(const QModelIndex &index, int role) const {
    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == AddressColumn)
            return addressAtIndex(index, m_port).toQUrl();
        break;
    case Qt::EditRole:
        if (index.column() == AddressColumn)
            return addressAtIndex(index).toQUrl();
        break;
    case Qt::CheckStateRole:
        if (index.column() == InUseColumn)
            return m_ignoredAddresses.contains(addressAtIndex(index)) ? Qt::Unchecked : Qt::Checked;
        break;
    case Qt::ForegroundRole:
        if (!isManualAddress(index) && index.column() == AddressColumn)
            return colorTheme()->color("light10");
        break;
    default:
        break;
    }

    return QVariant();
}

bool QnServerAddressesModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    Q_UNUSED(role)

    if (m_readOnly)
        return false;

    switch (index.column()) {
    case AddressColumn: {
        nx::utils::Url url = nx::utils::Url::fromUserInput(value.toString());
        url.setScheme(nx::network::http::kSecureUrlSchemeName);

        if (url.isEmpty())
            return false;

        if (!url.isValid()) {
            emit urlEditingFailed(index, InvalidUrl);
            return false;
        }

        if (url.port() == m_port)
            url.setPort(-1);

        if (m_addresses.contains(url) || m_ignoredAddresses.contains(url)) {
            emit urlEditingFailed(index, ExistingUrl);
            return false;
        }

        if (url.port() == -1) {
            nx::utils::Url explicitUrl = url;
            explicitUrl.setPort(m_port);

            if (m_addresses.contains(explicitUrl) || m_ignoredAddresses.contains(explicitUrl)) {
                emit urlEditingFailed(index, ExistingUrl);
                return false;
            }
        }

        if (index.row() < m_addresses.size())
            m_addresses[index.row()] = url;
        else
            m_manualAddresses[index.row() - m_addresses.size()] = url;

        emit dataChanged(index, index);

        return true;
    }
    case InUseColumn: {
        nx::utils::Url url = addressAtIndex(index);

        if (value.toInt() == Qt::Unchecked)
            m_ignoredAddresses.insert(url);
        else
            m_ignoredAddresses.remove(url);

        emit dataChanged(index, index);
        return true;
    }
    default:
        return false;
    }
}

QVariant QnServerAddressesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Vertical)
        return QVariant();

    if (role != Qt::DisplayRole)
        return QAbstractItemModel::headerData(section, orientation, role);

    switch (section) {
    case AddressColumn:
        return tr("Address");
    case InUseColumn:
        return QString();
    default:
        break;
    }

    return QVariant();
}

QModelIndex QnServerAddressesModel::index(int row, int column, const QModelIndex &parent) const {
    if (parent.isValid())
        return QModelIndex();

    return createIndex(row, column, row);
}

QModelIndex QnServerAddressesModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)

    return QModelIndex(); /* This is a plain list */
}

Qt::ItemFlags QnServerAddressesModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (isReadOnly())
        return flags;

    switch (index.column()) {
    case AddressColumn:
        if (index.row() >= m_addresses.size())
            flags |= Qt::ItemIsEditable;
        break;
    case InUseColumn:
        flags |= Qt::ItemIsUserCheckable;
        break;
    default:
        break;
    }

    return flags;
}

nx::utils::Url QnServerAddressesModel::addressAtIndex(const QModelIndex &index, int defaultPort) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QString();

    nx::utils::Url url;

    int idx = index.row();

    if (index.row() < m_addresses.size())
        url = m_addresses[index.row()];
    else {
        idx -= m_addresses.size();
        url = m_manualAddresses[idx];
    }

    if (url.port() == -1)
        url.setPort(defaultPort);

    return url;
}

QnSortedServerAddressesModel::QnSortedServerAddressesModel(QObject *parent) : QSortFilterProxyModel(parent) {}

bool QnSortedServerAddressesModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QnServerAddressesModel *model = dynamic_cast<QnServerAddressesModel*>(sourceModel());

    if (!model)
        return left.data().toString() < right.data().toString();

    bool lmanual = model->isManualAddress(left);
    bool rmanual = model->isManualAddress(right);

    if (lmanual != rmanual)
        return rmanual;

    return nx::utils::naturalStringLess(left.data().toString(), right.data().toString());
}
