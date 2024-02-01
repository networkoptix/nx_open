// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

/**
 * Mapping of all incorrect values to its location in LookupListEntriesModel
 * and mapping to required fix for use in QML.
 */
struct FixupEntry
{
    Q_GADGET
public:
    Q_PROPERTY(QVariantMap incorrectToCorrectWord MEMBER incorrectToCorrectWord)

    QVariantMap incorrectToCorrectWord;
    using Position = QPair<int, int>;
    std::map<Position, QString> positionToIncorrectWord;
};

/**
 * Raw presentation of first N rows from CSV, representing Lookup List Data.
 *
 * Stores association between column index and attribute name for Import preview.
 * Stores info about incorrect values, parsed from file.
 */
class NX_VMS_CLIENT_DESKTOP_API LookupListImportEntriesModel: public QAbstractTableModel
{
    Q_OBJECT
    using base_type = QAbstractTableModel;

public:
    /**
     * Raw presentation of CSV source file.
     * %example: [
     *  [Color, License Plate, Number],
     *  [red, "AA00A", 123]
     * ]
     */
    using PreviewRawData = std::vector<std::vector<QVariant>>;

    /**
     * Description on how and where fix import data by attrbuteName.
     * Have to use QVariantMap to use in QML.
     * Equivalent to QMap<QString, FixupEntry>;
     */
    using FixupData = QVariantMap;

    Q_PROPERTY(LookupListEntriesModel* lookupListEntriesModel READ lookupListEntriesModel
        WRITE setLookupListEntriesModel NOTIFY lookupListEntriesModelChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged);
    Q_PROPERTY(QString doNotImportText READ doNotImportText CONSTANT)
    Q_PROPERTY(FixupData fixupData READ fixupData NOTIFY fixupDataChanged)

    explicit LookupListImportEntriesModel(QAbstractTableModel* parent = nullptr);
    virtual ~LookupListImportEntriesModel() override;

    virtual QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE void setRawData(const PreviewRawData& rawData);
    Q_INVOKABLE void reset();

    void addLookupListEntry(QVariantMap& entry);
    bool fixupRequired();
    Q_INVOKABLE void applyFix();
    Q_INVOKABLE void revertImport();
    Q_INVOKABLE void addFixForWord(
        const QString& attributeName, const QString& incorrectWord, const QString& fix);

    Q_INVOKABLE void headerIndexChanged(int index, const QString& headerName);
    Q_INVOKABLE QMap<int, QString> columnIndexToAttribute();

    Q_INVOKABLE LookupListEntriesModel* lookupListEntriesModel();
    Q_INVOKABLE void setLookupListEntriesModel(LookupListEntriesModel* lookupListEntriesModel);

    Q_INVOKABLE FixupData fixupData();

    Q_INVOKABLE QString doNotImportText() const;

signals:
    void lookupListEntriesModelChanged();
    void rowCountChanged();
    void fixupDataChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
