#ifndef RESOURCE_TREE_WIDGET_H
#define RESOURCE_TREE_WIDGET_H

#include <QtCore/QAbstractItemModel>

#include <QtWidgets/QWidget>
#include <QtCore/QItemSelectionModel>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_criterion.h>

class QnResourceTreeItemDelegate;
class QnWorkbench;
class QSortFilterProxyModel;
class QnResourceTreeSortProxyModel;

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

        /** Set background opacity to items. */
        BackgroundOpacity = 0x2,

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

    QItemSelectionModel *selectionModel();

    const QnResourceCriterion &criterion() const;
    void setCriterion(const QnResourceCriterion &criterion);

    void setWorkbench(QnWorkbench *workbench);

    void edit();

    void expand(const QModelIndex &index);

    void expandAll();

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

    QAbstractItemView* treeView() const;

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;

    void updateCheckboxesVisibility();

signals:
    void activated(const QnResourcePtr &resource);
    void viewportSizeChanged();

    /**
     * This signal is emitted when the tree prepares to start a recursive operation
     * that may lead to a lot of dataChanged signals emitting.
     */
    void beforeRecursiveOperation();

    /**
     * This signal is emitted when the tree ends a recursive operation.
     */
    void afterRecursiveOperation();
private slots:
    void at_treeView_enterPressed(const QModelIndex &index);
    void at_treeView_spacePressed(const QModelIndex &index);
    void at_treeView_doubleClicked(const QModelIndex &index);
    void at_treeView_clicked(const QModelIndex &index);

    void at_resourceProxyModel_rowsInserted(const QModelIndex &parent, int start, int end);
    void at_resourceProxyModel_rowsInserted(const QModelIndex &index);

    void updateColumnsSize();
    void updateFilter();

private:
    QScopedPointer<Ui::QnResourceTreeWidget> ui;

    QnResourceCriterion m_criterion;

    QnResourceTreeItemDelegate *m_itemDelegate;

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

#endif // RESOURCE_TREE_WIDGET_H
