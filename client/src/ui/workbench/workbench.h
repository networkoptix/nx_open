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
 */
class QnWorkbench: public QObject {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING, /**< Viewing. This is the default mode. */
        EDITING  /**< Layout editing. */
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
     * \returns                         Currently selected item.
     */
    QnWorkbenchItem *selectedItem() const {
        return m_selectedItem;
    }

    /**
     * \param item                      New selected item for this workbench.
     */
    void setSelectedItem(QnWorkbenchItem *item);

    /**
     * \returns                         Currently zoomed item.
     */
    QnWorkbenchItem *zoomedItem() const {
        return m_zoomedItem;
    }

    /**
     * \param item                      New zoomed item for this workbench.
     */
    void setZoomedItem(QnWorkbenchItem *item);

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
     * This signal is emitted whenever the selected item of this workbench changes. 
     */
    void selectedItemChanged();

    /**
     * This signal is emitted whenever the zoomed item of this workbench changes.
     */
    void zoomedItemChanged();

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

    /** Currently selected item. NULL if none. */
    QnWorkbenchItem *m_selectedItem;

    /** Currently zoomed item. NULL if none. */
    QnWorkbenchItem *m_zoomedItem;

    /** Stored dummy layout. It is used to ensure that current layout is never NULL. */
    QnWorkbenchLayout *m_dummyLayout;
};

#endif // QN_WORKBENCH_H
