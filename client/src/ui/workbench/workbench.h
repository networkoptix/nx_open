#ifndef QN_WORKBENCH_H
#define QN_WORKBENCH_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>
#include <client/client_model_types.h>

#include "workbench_context_aware.h"

class QnWorkbenchLayout;
class QnWorkbenchGridMapper;
class QnWorkbenchItem;

// TODO: #Elric doxydoc is out of date

/**
 * Workbench ties layout, items and current UI-related "state" together.
 *
 * It abstracts away the fact that layout can be changed "on the fly" by
 * providing copies of layout signals that remove the necessity to
 * watch for layout changes.
 *
 * It also ensures that current layout is never NULL by replacing NULL layout
 * supplied by the user with internally stored empty layout.
 *
 * Workbench state consists of:
 * <ul>
 * <li>A list of layouts that are currently loaded.</li>
 * <li>Current layout that defines how items are placed.</li>
 * <li>A grid mapper that maps integer layout coordinates into floating-point
 *     surface coordinates.</li>
 * <li>Currently raised item - an item that is enlarged and is shown on top of other items.</li>
 * <li>Currently zoomed item - an item that is shown in full screen.</li>
 * </ul>
 */
class QnWorkbench: public QObject, public QnWorkbenchContextAware { // TODO: #Elric remove context-aware
    Q_OBJECT;
public:
    /**
     * Constructor.
     *
     * \param parent                    Parent object for this workbench.
     */
    QnWorkbench(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbench();

    /**
     * Clears this workbench by setting all of its properties to their initial
     * values.
     *
     * Note that this function does not reset parameters of the grid mapper.
     */
    void clear();

    /**
     * Note that this function never returns NULL.
     *
     * \returns                         Layout of this workbench.
     */
    QnWorkbenchLayout *currentLayout() const {
        return m_currentLayout;
    }

    /**
     * \returns                         Index of the current layout. May return -1 if dummy layout is currently in use.
     */
    int currentLayoutIndex() const {
        return layoutIndex(currentLayout());
    }

    /**
     * \param index                     Index of the layout to get.
     * \returns                         Layout for the given index. 
     */
    QnWorkbenchLayout *layout(int index) const;

    /**
     * \returns                         All layouts of this workbench. May be empty. 
     */
    const QList<QnWorkbenchLayout *> &layouts() const {
        return m_layouts;
    }

    /**
     * \param layout                    Layout to add to this workbench. 
     */
    void addLayout(QnWorkbenchLayout *layout);

    /**
     * \param layout                    Layout to insert into the list of this workbench's layouts.
     * \param index                     Position to insert at.
     */
    void insertLayout(QnWorkbenchLayout *layout, int index);

    /**
     * \param layout                    Layout to remove from the list of this workbench's layouts.
     */
    void removeLayout(QnWorkbenchLayout *layout);

    /**
     * \param index                     Index of the layout to remove from the list of this workbench's layouts.
     */
    void removeLayout(int index);

    /**
     * \param layout                    Layout to move to a new position in the list of this workbench's layouts.
     * \param index                     New position for the given layout.
     */
    void moveLayout(QnWorkbenchLayout *layout, int index);

    /**
     * \param layout                    Layout to find in the list of this workbench's layouts.
     * \returns                         Index of the given layout in the list of this workbench's layouts, or -1 if it is not there.
     */
    int layoutIndex(QnWorkbenchLayout *layout) const;

    /**
     * Note that workbench does not take ownership of the supplied layout.
     * If supplied layout is not in this workbench's layout list, 
     * it will be added to it.
     *
     * \param layout                    New layout for this workbench. If NULL
     *                                  is specified, a new empty layout owned by
     *                                  this workbench will be used.
     */
    void setCurrentLayout(QnWorkbenchLayout *layout);

    /**
     * Sets the index of the current layout. Note that index does not need
     * to be valid as it will be bounded to the closest valid value.
     *
     * \param index                     New current layout index. 
     */
    void setCurrentLayoutIndex(int index);

    /**
     * \returns                         Grid mapper for this workbench.
     */
    QnWorkbenchGridMapper *mapper() const {
        return m_mapper;
    }

    /**
     * \param role                      Role to get item for.
     * \returns                         Item for the given item role.
     */
    QnWorkbenchItem *item(Qn::ItemRole role);

    /**
     * \param role                      Role to set an item for.
     * \param item                      New item for the given role.
     */
    void setItem(Qn::ItemRole role, QnWorkbenchItem *item);

    void update(const QnWorkbenchState &state);
    void submit(QnWorkbenchState &state);

signals:
    /**
     * This signal is emitted while the workbench is still intact, but is about
     * to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted whenever the layout of this workbench is about to be changed.
     */
    void currentLayoutAboutToBeChanged();

    /**
     * This signal is emitted whenever the layout of this workbench changes.
     *
     * In most cases there is no need to listen to this signal as
     * workbench emits <tt>itemRemoved</tt> signal for each of the
     * old layout items and <tt>itemAdded</tt> for each of the new
     * layout items when layout is changed.
     */
    void currentLayoutChanged();

    /**
     * This signal is emitted whenever this workbench's layouts list changes. 
     */
    void layoutsChanged();

    /**
     * This signal is emitted whenever a new item is assigned to the role.
     *
     * \param role                      Item role.
     */
    void itemChanged(Qn::ItemRole role);

    /**
     * This signal is emitted whenever current cell aspect ratio changes.
     */
    void cellAspectRatioChanged();

    /**
     * This signal is emitted whenever current cell spacing changes.
     */
    void cellSpacingChanged();

private slots:
    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);
    void at_layout_aboutToBeDestroyed();
    void at_layout_cellAspectRatioChanged();
    void at_layout_cellSpacingChanged();

    void updateSingleRoleItem();
    void updateActiveRoleItem();
    void updateCentralRoleItem();

private:
    /** Current layout. */
    QnWorkbenchLayout *m_currentLayout;

    /** List of this workbench's layouts. */
    QList<QnWorkbenchLayout *> m_layouts;

    /** Grid mapper of this workbench. */
    QnWorkbenchGridMapper *m_mapper;

    /** Items by role. */
    QnWorkbenchItem *m_itemByRole[Qn::ItemRoleCount];

    /** Stored dummy layout. It is used to ensure that current layout is never NULL. */
    QnWorkbenchLayout *m_dummyLayout;
};

#endif // QN_WORKBENCH_H
