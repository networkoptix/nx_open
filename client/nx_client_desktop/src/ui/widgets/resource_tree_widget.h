#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

class QAbstractItemView;
class QSortFilterProxyModel;

class QnResourceItemDelegate;
class QnWorkbench;
class QnResourceTreeSortProxyModel;
class QnResourceTreeModelCustomColumnDelegate;

namespace Ui {
    class QnResourceTreeWidget;
}

namespace Qn {
    /**
     * Flags describing graphics tweaks usable in resource tree wigdet.
     */
    enum GraphicsTweaksFlag {
        /** Last row is hidden if there is not enough space to fully show it. */
        HideLastRow = 0x1,

        /** Set Qt::BypassGraphicsProxyWidget flag to items. */
        BypassGraphicsProxy = 0x4
    };
    Q_DECLARE_FLAGS(GraphicsTweaksFlags, GraphicsTweaksFlag)
}

class QnResourceTreeWidget : public QWidget {
    Q_OBJECT

    typedef QWidget base_type;

public:
    explicit QnResourceTreeWidget(QWidget *parent = 0);
    ~QnResourceTreeWidget();

    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *model);

    QSortFilterProxyModel* searchModel() const;

    QItemSelectionModel *selectionModel();
    void setWorkbench(QnWorkbench *workbench);

    void edit();

    void expand(const QModelIndex &index);

    void expandAll();

    /** Expands checked and partially checked nodes. */
    void expandChecked();

    QPoint selectionPos() const;

    /**
     * @brief setCheckboxesVisible      Show/hide checkboxes against each row.
     * @param visible                   Target state of checkboxes.
     */
    void setCheckboxesVisible(bool visible = true);

    /**
     * @brief isCheckboxesVisible       This property holds whether checkboxes against each row are visible.
     * @return                          True if checkboxes are visible.
     */
    bool isCheckboxesVisible() const;

    /**
     * @brief setGraphicsTweaks         Set various graphics tweaks for widget displaying.
     * @param flags                     Set of flags describing tweaks.
     */
    void setGraphicsTweaks(Qn::GraphicsTweaksFlags flags);

    /**
     * @brief graphicsTweaks         Which graphics tweaks are enabled for this widget.
     * @return                          Set of flags describing used tweaks.
     */
    Qn::GraphicsTweaksFlags graphicsTweaks();

    /**
     * @brief setFilterVisible          Show/hide resource tree filter widget.
     * @param visible                   Target state of filter.
     */
    void setFilterVisible(bool visible = true);

    /**
     * @brief isFilterVisible           This property holds whether filter widget is visible.
     * @return                          True if filter is visible.
     */
    bool isFilterVisible() const;

    /**
     * @brief setEditingEnabled         Enable or disable item editing: renaming by F2 and moving by drag'n'drop.
     * @param enabled                   Whether editing should be allowed.
     */
    void setEditingEnabled(bool enabled = true);

    /**
     * @brief isEditingEnabled          This property holds whether renaming by F2 and moving by drag'n'drop are enabled.
     * @return                          True if editing is allowed.
     */
    bool isEditingEnabled() const;

    void setSimpleSelectionEnabled(bool enabled = true);

    bool isSimpleSelectionEnabled() const;

    QnResourceTreeModelCustomColumnDelegate* customColumnDelegate() const;
    void setCustomColumnDelegate(QnResourceTreeModelCustomColumnDelegate *columnDelegate);

    /**
     * Allow some nodes to be auto-expanded. By default all nodes are auto-expanded.
     */
    using AutoExpandPolicy = std::function<bool(const QModelIndex&)>;
    void setAutoExpandPolicy(AutoExpandPolicy policy);

    QAbstractItemView* treeView() const;
    QnResourceItemDelegate* itemDelegate() const;

    using base_type::update;
    void update(const QnResourcePtr& resource);

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;

signals:
    void activated(const QModelIndex& index, bool withMouse);

    /**
     * This signal is emitted when the tree prepares to start a recursive operation
     * that may lead to a lot of dataChanged signals emitting.
     */
    void beforeRecursiveOperation();

    /**
     * This signal is emitted when the tree ends a recursive operation.
     */
    void afterRecursiveOperation();

    void filterEnterPressed();
    void filterCtrlEnterPressed();

private:
    void at_treeView_spacePressed(const QModelIndex &index);
    void at_treeView_clicked(const QModelIndex &index);

    void at_resourceProxyModel_rowsInserted(const QModelIndex& parent, int start, int end);
    void expandNodeIfNeeded(const QModelIndex& index);

    void initializeFilter();
    void updateColumns();
    void updateFilter();

    void expandCheckedRecursively(const QModelIndex& from);

    void updateShortcutHintVisibility();

private:
    QScopedPointer<Ui::QnResourceTreeWidget> ui;

    QnResourceItemDelegate *m_itemDelegate;

    AutoExpandPolicy m_autoExpandPolicy;

    QnResourceTreeSortProxyModel *m_resourceProxyModel;

    /** This property holds whether checkboxes against each row are visible. */
    bool m_checkboxesVisible;

    /** This property holds which graphics tweaks are used for widget displaying. */
    Qn::GraphicsTweaksFlags m_graphicsTweaksFlags;

    /** This property holds whether renaming by F2 and moving by drag'n'drop are enabled. */
    bool m_editingEnabled;

    bool m_simpleSelectionEnabled;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::GraphicsTweaksFlags)
