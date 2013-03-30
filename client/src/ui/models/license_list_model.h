#ifndef QN_LICENSE_LIST_MODEL_H
#define QN_LICENSE_LIST_MODEL_H

#include <QtGui/QStandardItemModel>

#include <licensing/license.h> // TODO: #Elric use fwd

class QnLicenseListModel: public QStandardItemModel {
    Q_OBJECT
    typedef QStandardItemModel base_type;

public:
    enum Column {
        TypeColumn,
        CameraCountColumn,
        LicenseKeyColumn,
        ExpirationDateColumn,
        ExpiresInColumn,
        ColumnCount
    };

    QnLicenseListModel(QObject *parent = NULL);
    virtual ~QnLicenseListModel();

    const QList<QnLicensePtr> &licenses() const;
    void setLicenses(const QList<QnLicensePtr> &licenses);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    QnLicensePtr license(const QModelIndex &index) const;

private:
    void rebuild();

    static QString columnTitle(Column column);
    static QStandardItem *createItem(Column column, const QnLicensePtr &license);

private:
    QList<Column> m_columns;
    QList<QnLicensePtr> m_licenses;
};

#endif // QN_LICENSE_LIST_MODEL_H
