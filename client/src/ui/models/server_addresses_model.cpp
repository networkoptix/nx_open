#include "server_addresses_model.h"

#include <QtCore/QSet>

#include "utils/common/util.h"

QnServerAddressesModel::QnServerAddressesModel(QObject *parent) :
    base_type(parent),
    m_port(-1)
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

void QnServerAddressesModel::setAddressList(const QList<QUrl> &addresses) {
    beginResetModel();
    m_addresses = addresses;
    endResetModel();
}

QList<QUrl> QnServerAddressesModel::addressList() const {
    return m_addresses;
}

void QnServerAddressesModel::setManualAddressList(const QList<QUrl> &addresses) {
    beginResetModel();
    m_manualAddresses = addresses;
    endResetModel();
}

QList<QUrl> QnServerAddressesModel::manualAddressList() const {
    return m_manualAddresses;
}

void QnServerAddressesModel::setIgnoredAddresses(const QSet<QUrl> &ignoredAddresses) {
    beginResetModel();
    m_ignoredAddresses = ignoredAddresses;
    endResetModel();
}

QSet<QUrl> QnServerAddressesModel::ignoredAddresses() const {
    return m_ignoredAddresses;
}

void QnServerAddressesModel::addAddress(const QUrl &url, bool isManualAddress) {
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

void QnServerAddressesModel::resetModel(const QList<QUrl> &addresses, const QList<QUrl> &manualAddresses, const QSet<QUrl> &ignoredAddresses, int port) {
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
            return addressAtIndex(index, m_port).toString();
        break;
    case Qt::EditRole:
        if (index.column() == AddressColumn)
            return addressAtIndex(index, m_port).toString();
        break;
    case Qt::CheckStateRole:
        if (index.column() == InUseColumn)
            return m_ignoredAddresses.contains(addressAtIndex(index)) ? Qt::Unchecked : Qt::Checked;
        break;
    case Qt::ForegroundRole:
        if (!isManualAddress(index) && index.column() == AddressColumn)
            return m_colors.readOnly;
        break;
    default:
        break;
    }

    return QVariant();
}

bool QnServerAddressesModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    Q_UNUSED(role)

    switch (index.column()) {
    case AddressColumn: {
        QUrl url = QUrl::fromUserInput(value.toString());
        if (!url.isValid())
            return false;

        if (url.port() == m_port)
            url.setPort(-1);

        if (m_addresses.contains(url) || m_ignoredAddresses.contains(url))
            return false;

        if (url.port() == -1) {
            QUrl explicitUrl = url;
            explicitUrl.setPort(m_port);

            if (m_addresses.contains(explicitUrl) || m_ignoredAddresses.contains(explicitUrl))
                return false;
        }

        if (index.row() < m_addresses.size())
            m_addresses[index.row()] = url;
        else
            m_manualAddresses[index.row() - m_addresses.size()] = url;

        return true;
    }
    case InUseColumn: {
        QUrl url = addressAtIndex(index);

        if (value.toInt() == Qt::Unchecked)
            m_ignoredAddresses.insert(url);
        else
            m_ignoredAddresses.remove(url);

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
        return tr("In Use");
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

const QnRoutingManagementColors QnServerAddressesModel::colors() const {
    return m_colors;
}

void QnServerAddressesModel::setColors(const QnRoutingManagementColors &colors) {
    m_colors = colors;
    if (!m_addresses.isEmpty() || m_manualAddresses.isEmpty())
        emit dataChanged(index(0, AddressColumn), index(m_addresses.size() + m_manualAddresses.size(), AddressColumn));
}

QUrl QnServerAddressesModel::addressAtIndex(const QModelIndex &index, int defaultPort) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QString();

    QUrl url;

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

    return left.data().toString() < right.data().toString();
}
