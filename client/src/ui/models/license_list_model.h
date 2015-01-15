#ifndef QN_LICENSE_LIST_MODEL_H
#define QN_LICENSE_LIST_MODEL_H

#include <QtGui/QStandardItemModel>

#include <client/client_color_types.h>

#include <licensing/license.h> // TODO: #Elric use fwd

#include <ui/customization/customized.h>

class QnLicenseListModel: public Customized<QStandardItemModel> {
    Q_OBJECT
    Q_PROPERTY(QnLicensesListModelColors colors READ colors WRITE setColors)
    typedef Customized<QStandardItemModel> base_type;

public:
    enum Column {
        TypeColumn,
        CameraCountColumn,
        LicenseKeyColumn,
        ExpirationDateColumn,
        LicenseStatusColumn,
        ServerColumn,
        ColumnCount
    };

    QnLicenseListModel(QObject *parent = NULL);
    virtual ~QnLicenseListModel();

    const QList<QnLicensePtr> &licenses() const;
    void setLicenses(const QList<QnLicensePtr> &licenses);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    QnLicensePtr license(const QModelIndex &index) const;

    const QnLicensesListModelColors colors() const;
    void setColors(const QnLicensesListModelColors &colors);
private:
    void rebuild();

    static QString columnTitle(Column column);
    static QStandardItem *createItem(Column column, const QnLicensePtr &license, const QnLicensesListModelColors &colors);

private:
    QnLicensesListModelColors m_colors;
    QList<Column> m_columns;
    QList<QnLicensePtr> m_licenses;
};

#endif // QN_LICENSE_LIST_MODEL_H
