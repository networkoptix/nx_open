#pragma once

#include <QtCore/QAbstractListModel>

#include <client/client_color_types.h>

#include <licensing/license.h> // TODO: #Elric use fwd

#include <ui/customization/customized.h>

class QnLicenseListModel : public Customized<QAbstractListModel>
{
    Q_OBJECT
    Q_PROPERTY(QnLicensesListModelColors colors READ colors WRITE setColors)

    using base_type = Customized<QAbstractListModel>;
public:
    enum Column
    {
        TypeColumn,
        CameraCountColumn,
        LicenseKeyColumn,
        ExpirationDateColumn,
        LicenseStatusColumn,
        ServerColumn,

        ColumnCount
    };

    enum DataRole
    {
        LicenseRole = Qt::UserRole + 1
    };

    QnLicenseListModel(QObject* parent = nullptr);
    virtual ~QnLicenseListModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    //virtual Qt::ItemFlags flags(const QModelIndex& index) const override;


    void updateLicenses(const QnLicenseList& licenses);

    QnLicensePtr license(const QModelIndex &index) const;
    void addLicense(const QnLicensePtr& license);
    void removeLicense(const QnLicensePtr& license);

    const QnLicensesListModelColors colors() const;
    void setColors(const QnLicensesListModelColors &colors);

private:
    QString getLicenseStatus(const QnLicensePtr& license) const;

private:
    QnLicensesListModelColors m_colors;
    QnLicenseList m_licenses;
};
