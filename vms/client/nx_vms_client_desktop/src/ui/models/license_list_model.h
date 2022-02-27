// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <licensing/license.h> // TODO: #sivanov use fwd
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

class QnLicenseListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum Column
    {
        TypeColumn,
        CameraCountColumn,
        LicenseKeyColumn,
        ExpirationDateColumn,
        ServerColumn,
        LicenseStatusColumn,

        ColumnCount
    };

    enum DataRole
    {
        LicenseRole = Qt::UserRole + 1
    };

    QnLicenseListModel(QObject* parent = nullptr);
    virtual ~QnLicenseListModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void updateLicenses(const QnLicenseList& licenses);

    void addLicense(const QnLicensePtr& license);
    void removeLicense(const QnLicensePtr& license);

    bool extendedStatus() const;
    void setExtendedStatus(bool value);

private:
    QVariant textData(const QModelIndex& index, bool fullText) const;
    QVariant foregroundData(const QModelIndex& index) const;
    QnLicensePtr license(const QModelIndex& index) const;

    enum ExpirationState
    {
        Expired,
        SoonExpires,
        Good
    };
    QPair<ExpirationState, QString> expirationInfo(const QnLicensePtr& license, bool fullText) const;
    QnMediaServerResourcePtr serverByLicense(const QnLicensePtr& license) const;

private:
    QnLicenseList m_licenses;
    bool m_extendedStatus;
};
