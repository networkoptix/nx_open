#ifndef QN_WORKBENCH_H
#define QN_WORKBENCH_H

#include <QObject>

class QnWorkbenchLayout;
class QnWorkbenchGridMapper;
class QnWorkbenchItem;

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
 * <li>Mode that determines which actions are allowed for workbench users.</>
 * <li>A layout that defines how items are placed.</li>
 * <li>A grid mapper that maps integer layout coordinates into floating-point
 *     surface coordinates.</li>
 * <li>Currently raised item - an item that is enlarged and is shown on top of other items.</li>
 * <li>Currently zoomed item - an item that is shown in full screen.</li>
 * <li>Currently focused item - an item that is manipulated by ui controls, such as play/pause buttons.</li>
 * </ul>
 */
class QnWorkbench: public QObject {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING, /**< Viewing. This is the default mode. */
        EDITING  /**< Layout editing. */
    };

    enum ItemRole {
        RAISED,  /**< The item is raised. */
        ZOOMED,  /**< The item is zoomed. */
        FOCUSED, /**< The item is focused. */
        ITEM_ROLE_COUNT
    };

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
    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    /**
     * Note that workbench does not take ownership of the supplied layout.
     *
     * \param layout                    New layout for this workbench. If NULL
     *                                  is specified, a new empty layout owned by
     *                                  this workbench will be used.
     */ 
    void setLayout(QnWorkbenchLayout *layout);

    /**
     * \returns                         Grid mapper for this workbench. 
     */
    QnWorkbenchGridMapper *mapper() const {
        return m_mapper;
    }

    /**
     * \returns                         Current mode. 
     */
    Mode mode() const {
        return m_mode;
    }
    
    /**
     * \param mode                      New mode for this workbench.
     */
    void setMode(Mode mode);

    /**
     * \param role                      Role to get item for.
     * \returns                         Item for the given item role.
     */
    QnWorkbenchItem *item(ItemRole role);

    /**
     * \param role                      Role to set an item for.
     * \param item                      New item for the given role.
     */
    void setItem(ItemRole role, QnWorkbenchItem *item);

signals:
    /**
     * This signal is emitted while the workbench is still intact, but is about 
     * to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted whenever the mode of this workbench changes.
     */
    void modeChanged();

    /**
     * This signal is emitted whenever a new item is assigned to the role. 
     * 
     * \param role                      Item role.
     */
    void itemChanged(QnWorkbench::ItemRole role);

    /**
     * This signal is emitted whenever the layout of this workbench changes.
     * 
     * In most cases there is be no need to listen to this signal as
     * workbench emits <tt>itemAboutToBeRemoved</tt> signal for each of the 
     * old layout items and <tt>itemAdded</tt> for each of the new
     * layout items when layout is changed.
     */
    void layoutChanged();

    /**
     * This signal is emitted whenever an item is added to this workbench's
     * layout.
     * 
     * \param item                      Item that was added.
     */
    void itemAdded(QnWorkbenchItem *item);

    /**
     * This signal is emitted whenever an item is about to be removed from 
     * this workbench's layout.
     * 
     * \param item                      Item that is about to be removed.
     */
    void itemAboutToBeRemoved(QnWorkbenchItem *item);

private slots:
    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemAboutToBeRemoved(QnWorkbenchItem *item);
    void at_layout_aboutToBeDestroyed();

private:
    /** Current layout. */
    QnWorkbenchLayout *m_layout;

    /** Grid mapper of this workbench. */
    QnWorkbenchGridMapper *m_mapper;

    /** Current mode. */
    Mode m_mode;

    /** Items by role. */
    QnWorkbenchItem *m_itemByRole[ITEM_ROLE_COUNT];

    /** Stored dummy layout. It is used to ensure that current layout is never NULL. */
    QnWorkbenchLayout *m_dummyLayout;
};

#endif // QN_WORKBENCH_H
