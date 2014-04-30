#include "license_list_model.h"

#include <cassert>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>

QnLicenseListModel::QnLicenseListModel(QObject *parent):
    base_type(parent)
{
    rebuild();
}

QnLicenseListModel::~QnLicenseListModel() {
    return;
}

const QList<QnLicensePtr> &QnLicenseListModel::licenses() const {
    return m_licenses;
}

void QnLicenseListModel::setLicenses(const QList<QnLicensePtr> &licenses) {
    if(m_licenses == licenses)
        return;

    m_licenses = licenses;

    rebuild();
}

QList<QnLicenseListModel::Column> QnLicenseListModel::columns() const {
    return m_columns;
}

void QnLicenseListModel::setColumns(const QList<Column> &columns) {
    if(m_columns == columns)
        return;

    foreach(Column column, columns) {
        if(column < 0 || column >= ColumnCount) {
            qnWarning("Invalid column '%1'.", static_cast<int>(column));
            return;
        }
    }

    m_columns = columns;

    rebuild();
}

const QnLicensesListModelColors QnLicenseListModel::colors() const {
    return m_colors;
}

void QnLicenseListModel::setColors(const QnLicensesListModelColors &colors) {
    m_colors = colors;
    rebuild();
}


QString QnLicenseListModel::columnTitle(Column column) {
    switch(column) {
    case TypeColumn:            return tr("Type");
    case CameraCountColumn:     return tr("Cameras");
    case LicenseKeyColumn:      return tr("License Key");
    case ExpirationDateColumn:  return tr("Expiration Date");
    case ExpiresInColumn:       return tr("Expires in");
    default:
        assert(false);
        return QString();
    }
}

QStandardItem *QnLicenseListModel::createItem(Column column, const QnLicensePtr &license, const QnLicensesListModelColors &colors) {
    QStandardItem *item = new QStandardItem();
    item->setData(QBrush(colors.normal), Qt::ForegroundRole);

    switch(column) {
    case TypeColumn:
        item->setText(license->typeName());
        break;
    case CameraCountColumn:
        item->setText(QString::number(license->cameraCount()));
        break;
    case LicenseKeyColumn:
        item->setText(QLatin1String(license->key()));
        break;
    case ExpirationDateColumn:
        item->setText(license->expirationTime() < 0 ? tr("Never") : QDateTime::fromMSecsSinceEpoch(license->expirationTime()).toString());
        break;
    case ExpiresInColumn: {
        qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
        qint64 expirationTime = license->expirationTime();

        qint64 day = 1000ll * 60ll * 60ll * 24ll;
        qint64 timeLeft = expirationTime - currentTime;
        if(timeLeft < 0) {
            item->setText(tr("Expired"));
            item->setData(QBrush(colors.expired), Qt::ForegroundRole);
        } else {
            if(timeLeft < 5 * day)
                item->setData(QBrush(colors.warning), Qt::ForegroundRole);

            int daysLeft = QDateTime::fromMSecsSinceEpoch(currentTime).date().daysTo(QDateTime::fromMSecsSinceEpoch(expirationTime).date());
            if(daysLeft == 0) {
                item->setText(tr("Today"));
            } else if(daysLeft == 1) {
                item->setText(tr("Tomorrow"));
            } else {
                item->setText(tr("In %n days", 0, daysLeft));
            }
        }
        break;
    }
    default:
        assert(false);
    }

    if (!qnLicensePool->isLicenseValid(license))
        item->setData(QBrush(colors.expired), Qt::ForegroundRole);

    return item;
}

void QnLicenseListModel::rebuild() {
    clear();
    if(m_columns.isEmpty())
        return;

    /* Fill header. */
    setColumnCount(m_columns.size());
    for(int c = 0; c < m_columns.size(); c++)
        setHeaderData(c, Qt::Horizontal, columnTitle(m_columns[c]));

    /* Fill data. */
    for(int r = 0; r < m_licenses.size(); r++) {
        QList<QStandardItem *> items;
        for(int c = 0; c < m_columns.size(); c++)
            items.push_back(createItem(m_columns[c], m_licenses[r], m_colors));
        items[0]->setData(r, Qt::UserRole);

        appendRow(items);
    }
}

QnLicensePtr QnLicenseListModel::license(const QModelIndex &index) const {
    bool ok = false;
    int r = index.sibling(index.row(), 0).data(Qt::UserRole).toInt(&ok);
    if(!ok || r < 0 || r >= m_licenses.size())
        return QnLicensePtr();

    return m_licenses[r];
}
