#ifndef QN_INSTRUMENT_MANAGER_H
#define QN_INSTRUMENT_MANAGER_H

#include <QObject>
#include <QSet>
#include <QList>
#include "instrument.h"

class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;

class InstrumentManagerPrivate;

/**
 * Instrument manager ties graphics views and instruments together.
 * 
 * It supports multiple graphics views per instrument manager.
 */
class InstrumentManager: public QObject {
    Q_OBJECT;

public:
    /**
     * Constructor.
     * 
     * \param scene                    Graphics scene that this instrument manager 
     *                                 will work with.
     * \param parent                   Parent object for this instrument manager.
     */
    InstrumentManager(QObject *parent = NULL);

    /**
     * Destructor.
     */
    virtual ~InstrumentManager();

    /**
     * Installs the given instrument to the associated graphics view.
     * 
     * \param instrument               Instrument to install.
     * \returns                        Whether the instrument was successfully installed.
     */
    bool installInstrument(Instrument *instrument);

    /**
     * Uninstalls the given instrument from the associated graphics view.
     * 
     * \param instrument               Instrument to uninstall.
     * \returns                        Whether the instrument was successfully uninstalled.
     */
    bool uninstallInstrument(Instrument *instrument);

    /**
     * Registers the given graphics scene with this instrument manager.
     * 
     * If the given scene is destroyed, it will be unregistered automatically.
     *
     * \param scene                    Graphics scene to register.
     */
    void registerScene(QGraphicsScene *scene);

    /**
     * Unregisters the given graphics scene from this instrument manager.
     * 
     * Unregistering a scene automatically unregisters all its views and items.
     * 
     * \param scene                    Graphics scene to unregister.
     */
    void unregisterScene(QGraphicsScene *scene);

    /**
     * Registers the given graphics view with this instrument manager.
     *
     * If the given view is destroyed, it will be unregistered automatically.
     *
     * \param view                     Graphics view to register.
     */
    void registerView(QGraphicsView *view);

    /**
     * Unregisters the given graphics view from this instrument manager.
     * 
     * \param view                     Graphics view to unregister.
     */
    void unregisterView(QGraphicsView *view);

    /**
     * Registers the given graphics item with this instrument manager.
     *
     * Note that the given item won't be unregistered automatically if it is
     * destroyed. Unregistering it is up to the user.
     *
     * \param item                     Graphics item to register.
     */
    void registerItem(QGraphicsItem *item);

    /**
     * Unregisters the given graphics item from this instrument manager.
     * 
     * \param item                     Graphics item to unregister.
     */
    void unregisterItem(QGraphicsItem *item);

    /**
     * \returns                        Graphics scene that is registered with 
     *                                 this instrument manager.
     */
    QGraphicsScene *scene() const;

    /**
     * \returns                        Graphics views that are registered with 
     *                                 this instrument manager.
     */
    const QSet<QGraphicsView *> &views() const;

    /**
     * \returns                        Graphics items that are registered with
     *                                 this instrument manager.
     */
    const QSet<QGraphicsItem *> &items() const;

    /**
     * \returns                        All instruments installed into this 
     *                                 instrument manager.
     */
    const QList<Instrument *> &instruments() const;

    /**
     * \tparam                         Type of the instrument to get.
     * \returns                        The first instrument in instrument queue
     *                                 that has the given type, or NULL if no such
     *                                 instrument exists.
     */
    template<class T>
    T *instrument() const {
        const QList<Instrument *> &instruments = this->instruments();

        for (int i = instruments.size() - 1; i >= 0; i--) {
            T *result = dynamic_cast<T *>(instruments[i]);
            if(result != NULL)
                return result;
        }

        return NULL;
    }

    /**
     * \param scene                    Scene.
     * \returns                        List of all instrument managers managing
     *                                 the given scene.
     */
    static QList<InstrumentManager *> managersOf(QGraphicsScene *scene);

private slots:
    void at_view_destroyed(QObject *view);
    void at_viewport_destroyed(QObject *viewport);

private:
    InstrumentManagerPrivate *const d_ptr;

private:
    Q_DECLARE_PRIVATE(InstrumentManager);
};

#endif // QN_INSTRUMENT_MANAGER_H
