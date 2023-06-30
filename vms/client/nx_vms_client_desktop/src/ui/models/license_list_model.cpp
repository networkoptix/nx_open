// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_list_model.h"

#include <QtGui/QBrush>

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/license/validator.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/synctime.h>

namespace {

/** How many ms before expiration should we show warning. Default value is 15 days. */
const qint64 kExpirationWarningTimeMs = 15ll * 1000ll * 60ll * 60ll * 24ll;

} // namespace

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::license;

QnLicenseListModel::QnLicenseListModel(QObject* parent) :
    base_type(parent),
    m_extendedStatus(false)
{
}

QnLicenseListModel::~QnLicenseListModel()
{
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
    if (index.model() != this)
        return QVariant();

    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return textData(index, false);

        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleDescriptionRole:
        case Qt::ToolTipRole:
            return textData(index, true);

        case Qt::DecorationRole:
        {
            if (index.column() != ServerColumn)
                break;
            if (auto server = serverByLicense(license(index)))
                return qnResIconCache->icon(server);
            break;
        }

        case LicenseRole:
            return QVariant::fromValue(license(index));

        case core::ResourceRole:
            if (index.column() == ServerColumn)
                return QVariant::fromValue<QnResourcePtr>(serverByLicense(license(index)));
            break;

        case Qt::ForegroundRole:
            return foregroundData(index);

        default:
            break;
    }

    return QVariant();
}

QVariant QnLicenseListModel::textData(const QModelIndex& index, bool fullText) const
{
    auto license = this->license(index);
    switch (index.column())
    {
        case TypeColumn:
            return license->displayName();

        case CameraCountColumn:
            return QString::number(license->cameraCount());

        case LicenseKeyColumn:
            return QString::fromLatin1(license->key());

        case ExpirationDateColumn:
            return license->neverExpire()
                ? tr("Never")
                : nx::vms::time::toString(QDateTime::fromMSecsSinceEpoch(license->expirationTime()));

        case LicenseStatusColumn:
        {
            bool fullStatus = fullText || m_extendedStatus;
            Validator validator(systemContext());
            QnLicenseErrorCode code = validator.validate(license);
            if (code == QnLicenseErrorCode::NoError)
                return expirationInfo(license, fullStatus).second;

            if (fullStatus)
                return Validator::errorMessage(code, license->type());

            switch (code)
            {
                case QnLicenseErrorCode::Expired:
                    return tr("Expired");
                case QnLicenseErrorCode::TemporaryExpired:
                    return tr("Not verified");
                default:
                    return tr("Error");
            }
        }

        case ServerColumn:
        {
            if (license->type() == Qn::LC_SaasLocalRecording)
                return nx::branding::cloudHost();
            auto server = serverByLicense(license);
            if (!server)
                return tr("Server not found");
            return QnResourceDisplayInfo(server).toString(fullText
                ? Qn::RI_WithUrl
                : Qn::RI_NameOnly);
        }

        default:
            break;
    }

    return QVariant();
}

QVariant QnLicenseListModel::foregroundData(const QModelIndex& index) const
{
    auto license = this->license(index);
    switch (index.column())
    {
        case QnLicenseListModel::ServerColumn:
        {
            if (!serverByLicense(license))
                return QBrush(core::colorTheme()->color("red_l2"));
            break;
        }

        case QnLicenseListModel::ExpirationDateColumn:
        case QnLicenseListModel::LicenseStatusColumn:
        {
            Validator validator(systemContext());
            QnLicenseErrorCode code = validator.validate(license);
            if (code != QnLicenseErrorCode::NoError)
            {
                if (index.column() != QnLicenseListModel::ExpirationDateColumn
                    || code == QnLicenseErrorCode::Expired)
                {
                    return QBrush(core::colorTheme()->color("red_l2"));
                }
            }

            switch (expirationInfo(license, false).first)
            {
                case Expired:
                    return QBrush(core::colorTheme()->color("red_l2"));
                case SoonExpires:
                    return QBrush(core::colorTheme()->color("yellow_core"));
                default:
                    break;
            }

            break;
        }

        default:
            break;
    }

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
        case TypeColumn:
            return tr("Type");

        case CameraCountColumn:
            return tr("Channels");

        case LicenseKeyColumn:
            return tr("License Key");

        case ExpirationDateColumn:
            return tr("Expires");

        case LicenseStatusColumn:
            return tr("Status");

        case ServerColumn:
            return tr("Server");

        default:
            break;
    }

    return QVariant();
}

QnLicensePtr QnLicenseListModel::license(const QModelIndex& index) const
{
    NX_ASSERT(index.model() == this);
    return m_licenses[index.row()];
}

void QnLicenseListModel::updateLicenses(const QnLicenseList& licenses)
{
    /* setLicenses() is called quite often, so we optimize it to maintain exiting selection model. */

    QSet<QnLicensePtr> existing = nx::utils::toQSet(m_licenses);
    QSet<QnLicensePtr> updated = nx::utils::toQSet(licenses);

    QSet<QnLicensePtr> toAdd = updated - existing;
    QSet<QnLicensePtr> toRemove = existing - updated;

    for (const QnLicensePtr& license: toRemove)
        removeLicense(license);

    for (const QnLicensePtr& license : toAdd)
        addLicense(license);
}

void QnLicenseListModel::addLicense(const QnLicensePtr& license)
{
    int count = m_licenses.size();
    ScopedInsertRows insertRows(this, QModelIndex(), count, count);
    m_licenses << license;
}

void QnLicenseListModel::removeLicense(const QnLicensePtr& license)
{
    int idx = m_licenses.indexOf(license);
    if (idx < 0)
        return;
    ScopedRemoveRows removeRows(this, QModelIndex(), idx, idx);
    m_licenses.removeAt(idx);
}

QPair<QnLicenseListModel::ExpirationState, QString> QnLicenseListModel::expirationInfo(
    const QnLicensePtr& license, bool fullText) const
{
    auto goodLicense =
        [fullText]() -> QPair<ExpirationState, QString>
        {
            return { Good, fullText
                ? tr("License is active")
                : tr("OK") };
        };

    auto expiredLicense =
        [fullText]() -> QPair<ExpirationState, QString>
        {
            return { Expired, fullText
                ? tr("License is expired")
                : tr("Expired") };
        };

    if (license->neverExpire())
        return goodLicense();

    qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    qint64 expirationTimeMs = license->expirationTime();

    qint64 timeLeftMs = expirationTimeMs - currentTimeMs;
    if (timeLeftMs >= kExpirationWarningTimeMs)
        return goodLicense();

    if (timeLeftMs <= 0)
        return expiredLicense();

    if (!fullText)
        return{ SoonExpires, tr("Expires soon") };

    int daysLeft = QDateTime::fromMSecsSinceEpoch(currentTimeMs).date().daysTo(
        QDateTime::fromMSecsSinceEpoch(expirationTimeMs).date());

    NX_ASSERT(daysLeft >= 0);
    QString message;

    switch (daysLeft)
    {
        case 0:
            message = tr("License expires today");
            break;
        case 1:
            message = tr("License expires tomorrow");
            break;
        default:
            message = tr("License expires in %n days", "", daysLeft);
            break;
    }

    return{ SoonExpires, message };
}

QnMediaServerResourcePtr QnLicenseListModel::serverByLicense(const QnLicensePtr& license) const
{
    Validator validator(systemContext());
    auto serverId = validator.serverId(license);
    return resourcePool()->getResourceById<QnMediaServerResource>(serverId);
}

bool QnLicenseListModel::extendedStatus() const
{
    return m_extendedStatus;
}

void QnLicenseListModel::setExtendedStatus(bool value)
{
    if (m_extendedStatus == value)
        return;

    m_extendedStatus = value;

    auto numRows = rowCount();
    if (numRows == 0)
        return;

    emit dataChanged(index(0, LicenseStatusColumn), index(numRows - 1, LicenseStatusColumn));
}
