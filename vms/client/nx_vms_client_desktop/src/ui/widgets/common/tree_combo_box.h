// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QTreeView>

#include <nx/utils/pending_operation.h>

class QnTreeComboBox : public QComboBox
{
    Q_OBJECT
    typedef QComboBox base_type;

    Q_PROPERTY(bool resizeViewToContents READ resizeViewToContents WRITE setResizeViewToContents)

public:
    explicit QnTreeComboBox(QWidget* parent = nullptr);
    virtual void showPopup() override;

    void setModel(QAbstractItemModel* model);

    /**
     * Current selected model index (shown in combobox main line).
     */
    QModelIndex currentModelIndex() const;

    /**
     * Current highlighted model index (highlighted in drop-down list when opened).
     */
    QModelIndex currentHighlightedIndex() const;

    QModelIndex findData(const QVariant& data, int role = Qt::UserRole, Qt::MatchFlags flags
        = Qt::MatchFlags(Qt::MatchExactly | Qt::MatchCaseSensitive)) const;

    /** Resize dropdown view to contents so it can exceed combo box maximum width. */
    void setResizeViewToContents(bool enabled);
    bool resizeViewToContents() const;

protected:
    virtual QSize minimumSizeHint() const override;

    virtual void wheelEvent(QWheelEvent* e) override;
    virtual void keyPressEvent(QKeyEvent* e) override;
    void setCurrentIndex(int index);

public slots:
    void setCurrentIndex(const QModelIndex& index);

signals:
    void currentIndexChanged(const QModelIndex& index);

private:
    QSize recomputeSizeHint(QSize& sh) const;
    int getTreeWidth(const QModelIndex& parent, int nestedLevel) const;
    Q_SLOT void at_currentIndexChanged(int index);
    void updateHighlight();

private:
    QTreeView* m_treeView;
    mutable QSize m_minimumSizeHint;
    bool m_resizeViewToContents = false;
    QPersistentModelIndex m_currentRoot;
    QMetaObject::Connection m_expandConnection;
    nx::utils::PendingOperation* m_updateHighlight;
};
