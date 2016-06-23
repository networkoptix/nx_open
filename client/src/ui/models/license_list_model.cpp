#include "license_list_model.h"

#include <cassert>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/style/resource_icon_cache.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include "utils/math/math.h"

namespace {

/** How many ms before expiration should we show warning. Default value is 15 days. */
const qint64 kExpirationWarningTimeMs = 15ll * 1000ll * 60ll * 60ll * 24ll;

}

QnLicenseListModel::QnLicenseListModel(QObject *parent) :
    base_type(parent)
{}

QnLicenseListModel::~QnLicenseListModel()
{
    return;
}

int QnLicenseListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_licenses.size();
}

int QnLicenseListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

QVariant QnLicenseListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnLicensePtr license = m_licenses[index.row()];
    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(license->serverId());

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
        case Qt::ToolTipRole:
            switch (index.column())
            {
                case TypeColumn:
                    return license->displayName();

                case CameraCountColumn:
                    return QString::number(license->cameraCount());

                case LicenseKeyColumn:
                    return QString::fromLatin1(license->key());

                case ExpirationDateColumn:
                    return license->expirationTime() < 0
                        ? tr("Never")
                        : QDateTime::fromMSecsSinceEpoch(license->expirationTime()).toString(Qt::SystemLocaleShortDate);

                case LicenseStatusColumn:
                    return getLicenseStatus(license);

                case ServerColumn:
                    return server
                        ? QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl)
                        : tr("<Server not found>");

                default:
                    break;
            } // switch (column)
            break;

        case Qt::TextAlignmentRole:
            switch (index.column())
            {
                case CameraCountColumn:
                    return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

                default:
                    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            } // switch (column)
            break;

        case Qt::ForegroundRole:
        {
            if (!qnLicensePool->isLicenseValid(license))
                return QBrush(m_colors.expired);

            switch (index.column())
            {
                case LicenseStatusColumn:
                {
                    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
                    qint64 expirationTime = license->expirationTime();

                    qint64 timeLeft = expirationTime - currentTime;
                    if (expirationTime > 0 && timeLeft < 0)
                        return QBrush(m_colors.expired);

                    if (expirationTime > 0 && timeLeft < kExpirationWarningTimeMs)
                        return QBrush(m_colors.warning);

                    break;
                }

                case ServerColumn:
                    if (!server)
                        return QBrush(m_colors.expired);
                    break;

                default:
                    break;
            } // switch (column)
            return QBrush(m_colors.normal);
        }

        case Qt::DecorationRole:
            if (index.column() == ServerColumn && server)
                return qnResIconCache->icon(QnResourceIconCache::Server);
            break;

        case LicenseRole:
            return qVariantFromValue(license);

        default:
            break;
    } // switch (role)

    return QVariant();
}

QVariant QnLicenseListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();

    if (section >= ColumnCount)
        return QVariant();

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case TypeColumn:            return tr("Type");
        case CameraCountColumn:     return tr("Amount");
        case LicenseKeyColumn:      return tr("License Key");
        case ExpirationDateColumn:  return tr("Expiration Date");
        case LicenseStatusColumn:   return tr("Status");
        case ServerColumn:          return tr("Server");
        default:
            break;
    }
    return QVariant();
}

void QnLicenseListModel::updateLicenses(const QnLicenseList& licenses)
{
    /* setLicenses() is called quite often, so we optimize it to maintain exiting selection model. */

    QSet<QnLicensePtr> existing = m_licenses.toSet();
    QSet<QnLicensePtr> updated = licenses.toSet();

    QSet<QnLicensePtr> toAdd = updated - existing;
    QSet<QnLicensePtr> toRemove = existing - updated;

    for (const QnLicensePtr& license: toRemove)
        removeLicense(license);

    for (const QnLicensePtr& license : toAdd)
        addLicense(license);
}

const QnLicensesListModelColors QnLicenseListModel::colors() const
{
    return m_colors;
}

void QnLicenseListModel::setColors(const QnLicensesListModelColors &colors)
{
    beginResetModel();
    m_colors = colors;
    endResetModel();
}

QString QnLicenseListModel::getLicenseStatus(const QnLicensePtr& license) const
{
    QnLicense::ErrorCode errCode;
    if (!qnLicensePool->isLicenseValid(license, &errCode))
        return license->errorMessage(errCode);

    qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    qint64 expirationTimeMs = license->expirationTime();

    qint64 timeLeftMs = expirationTimeMs - currentTimeMs;
    if (expirationTimeMs > 0 && timeLeftMs < 0)
        return tr("Expired");

    if (expirationTimeMs > 0 && timeLeftMs < kExpirationWarningTimeMs)
    {
        int daysLeft = QDateTime::fromMSecsSinceEpoch(currentTimeMs).date().daysTo(QDateTime::fromMSecsSinceEpoch(expirationTimeMs).date());
        if (daysLeft == 0)
            return tr("Today");

        if (daysLeft == 1)
            return tr("Tomorrow");

        return tr("In %n days", "", daysLeft);
    }

    return tr("OK");
}

QnLicensePtr QnLicenseListModel::license(const QModelIndex &index) const
{
    if (!index.isValid())
        return QnLicensePtr();
    return data(index, LicenseRole).value<QnLicensePtr>();
}

void QnLicenseListModel::addLicense(const QnLicensePtr& license)
{
    int count = m_licenses.size();
    beginInsertRows(QModelIndex(), count, count);
    m_licenses << license;
    endInsertRows();
}

void QnLicenseListModel::removeLicense(const QnLicensePtr& license)
{
    int idx = m_licenses.indexOf(license);
    if (idx < 0)
        return;
    beginRemoveRows(QModelIndex(), idx, idx);
    m_licenses.removeAt(idx);
    endRemoveRows();
}
