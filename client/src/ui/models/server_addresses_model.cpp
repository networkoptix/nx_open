#include "server_addresses_model.h"

#include <QtCore/QSet>

QnServerAddressesModel::QnServerAddressesModel(QObject *parent) :
    QAbstractItemModel(parent)
{
}

void QnServerAddressesModel::setAddressList(const QStringList &addresses) {
    beginResetModel();
    m_addresses = addresses;
    endResetModel();
}

QStringList QnServerAddressesModel::addressList() const {
    return m_addresses;
}

void QnServerAddressesModel::setManualAddressList(const QStringList &addresses) {
    beginResetModel();
    m_manualAddresses = addresses;
    endResetModel();
}

QStringList QnServerAddressesModel::manualAddressList() const {
    return m_manualAddresses;
}

void QnServerAddressesModel::setIgnoredAddresses(const QSet<QString> &ignoredAddresses) {
    beginResetModel();
    m_ignoredAddresses = ignoredAddresses;
    endResetModel();
}

QSet<QString> QnServerAddressesModel::ignoredAddresses() const {
    return m_ignoredAddresses;
}

void QnServerAddressesModel::resetModel(const QStringList &addresses, const QStringList &manualAddresses, const QSet<QString> &ignoredAddresses) {
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
            return isManualAddress(index) ? m_manualAddresses[index.row() - m_addresses.size()] : m_addresses[index.row()];
        break;
    case Qt::CheckStateRole:
        if (index.column() == IgnoredColumn)
            return !isManualAddress(index) && m_ignoredAddresses.contains(m_addresses[index.row()]) ? Qt::Checked : Qt::Unchecked;
        break;
    default:
        break;
    }

    return QVariant();
}

bool QnServerAddressesModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    Q_UNUSED(role)

    if (index.column() != IgnoredColumn)
        return false;

    if (isManualAddress(index))
        return false;

    emit ignoreChangeRequested(index.sibling(index.row(), AddressColumn).data().toString(), value.toInt() == Qt::Checked);

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
    case IgnoredColumn:
        return tr("Ignored");
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

    if (index.column() == IgnoredColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
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
