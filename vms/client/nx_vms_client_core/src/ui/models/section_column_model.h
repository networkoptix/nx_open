// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QPointer>

namespace nx::vms::client::core {

/**
 * This model allows to have a grid-like view of the source model with sections. Each row has up to
 * `columns` items all belonging the same section. Row is represented as a single item with
 * `display` role containing a JS array of data from the source model.
 * This model doesn't properly track row changes in persistent indexes when source model's layout
 * is changed or rows are moved.
 */
class NX_VMS_CLIENT_CORE_API SectionColumnModel: public QAbstractListModel
{
    Q_OBJECT

    using base_type = QAbstractListModel;

    Q_PROPERTY(QString sectionProperty
        READ sectionProperty
        WRITE setSectionProperty
        NOTIFY sectionPropertyChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
    Q_PROPERTY(QAbstractItemModel* sourceModel
        READ sourceModel
        WRITE setSourceModel
        NOTIFY sourceModelChanged)

public:
    enum RoleId
    {
        SectionRole = Qt::UserRole + 1,
    };

public:
    SectionColumnModel(QObject* parent = nullptr): base_type(parent) {}
    virtual ~SectionColumnModel();

    void setSectionProperty(const QString& sectionProperty);
    QString sectionProperty() const { return m_sectionProperty; }

    void setColumns(int columns);
    int columns() const { return m_columns; }

    void setSourceModel(QAbstractItemModel* sourceModel);
    QAbstractItemModel* sourceModel() const { return m_sourceModel; }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

signals:
    void sectionPropertyChanged();
    void columnsChanged();
    void sourceModelChanged();

private:
    void updateModel();
    void updateSections();
    int findSectionRole() const;

private:
    QString m_sectionProperty;
    int m_sectionRole = -1;
    int m_columns = 1;

    QPointer<QAbstractItemModel> m_sourceModel;

    struct Section
    {
        int startRow;
        int startSourceRow;
        QString section;
    };

    struct Mapping
    {
        std::vector<Section> sections;
        int rowCount = 0;
        int columns = 1;

        void clear()
        {
            sections.clear();
            rowCount = 0;
        }

        int toSourceRow(int row) const;
        int fromSourceRow(int sourceRow) const;

        static Mapping fromModel(QAbstractItemModel* model, int sectionRole, int columns);
    };

    Mapping m_mapping;
};

} // namespace nx::vms::client::core
