#include "server_addresses_model.h"

#include <QtCore/QSet>

QnServerAddressesModel::QnServerAddressesModel(QObject *parent) :
    base_type(parent)
{
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

void QnServerAddressesModel::resetModel(const QList<QUrl> &addresses, const QList<QUrl> &manualAddresses, const QSet<QUrl> &ignoredAddresses) {
    beginResetModel();
    m_addresses = addresses;
    m_manualAddresses = manualAddresses;
    m_ignoredAddresses = ignoredAddresses;
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
            return addressAtIndex(index).toString();
        break;
    case Qt::EditRole:
        if (index.column() == AddressColumn)
            return addressAtIndex(index);
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

    if (index.column() != InUseColumn)
        return false;

    emit ignoreChangeRequested(index.sibling(index.row(), AddressColumn).data().toString(), value.toInt() == Qt::Unchecked);

    return false;
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

    if (index.column() == InUseColumn)
        flags |= Qt::ItemIsUserCheckable;

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

QUrl QnServerAddressesModel::addressAtIndex(const QModelIndex &index) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QString();

    if (index.row() < m_addresses.size())
        return m_addresses[index.row()];
    else
        return m_manualAddresses[index.row() - m_addresses.size()];
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
