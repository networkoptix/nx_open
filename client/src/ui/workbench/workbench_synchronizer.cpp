#include "workbench_synchronizer.h"
#include <cassert>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/warnings.h>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include "workbench.h"
#include "workbench_layout.h"
#include "workbench_layout_synchronizer.h"

Q_DECLARE_METATYPE(QnLayoutItemDataMap)

namespace {
    const char *qn_createdLocallyPropertyName = "_qn_createdLocally";
    const char *qn_savedItemsPropertyName = "_qn_savedItems";

    void setCreatedLocally(QnWorkbenchLayout *layout, bool createdLocally) {
        layout->setProperty(qn_createdLocallyPropertyName, createdLocally);
    }

    bool isCreatedLocally(QnWorkbenchLayout *layout) {
        return layout->property(qn_createdLocallyPropertyName).toBool();
    }

    QnLayoutItemDataMap savedItems(const QnLayoutResourcePtr &resource) {
        return resource->property(qn_savedItemsPropertyName).value<QnLayoutItemDataMap>();
    }

    void setSavedItems(const QnLayoutResourcePtr &resource, const QnLayoutItemDataMap &items) {
        resource->setProperty(qn_savedItemsPropertyName, QVariant::fromValue(items));
    }

    bool hasSavedItems(const QnLayoutResourcePtr &resource) {
        return !resource->property(qn_savedItemsPropertyName).isNull();
    }

} // anonymous namespace


void detail::WorkbenchSynchronizerReplyProcessor::at_finished(int status, const QByteArray &errorString, QnResourceList, int) {
    if(status == 0)
        setSavedItems(m_resource, m_resource->getItems());

    emit finished(status, errorString, m_resource);

    deleteLater();
}


QnWorkbenchSynchronizer::QnWorkbenchSynchronizer(QObject *parent):
    QObject(NULL),
    m_running(false),
    m_workbench(NULL),
    m_update(false),
    m_submit(false),
    m_connection(QnAppServerConnectionFactory::createConnection())
{}

QnWorkbenchSynchronizer::~QnWorkbenchSynchronizer() {}

void QnWorkbenchSynchronizer::setWorkbench(QnWorkbench *workbench) {
    if(m_workbench == workbench)
        return;

    if(m_workbench != NULL && !m_user.isNull())
        stop();

    m_workbench = workbench;

    if(m_workbench != NULL && !m_user.isNull())
        start();
}

void QnWorkbenchSynchronizer::setUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    if(m_workbench != NULL && !m_user.isNull())
        stop();

    m_user = user;

    if(m_workbench != NULL && !m_user.isNull())
        start();
}

QnLayoutResourcePtr QnWorkbenchSynchronizer::checkLayoutResource(QnWorkbenchLayout *layout) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return QnLayoutResourcePtr();
    }

    if(!m_running) {
        qnWarning("Synchronizer is not running.");
        return QnLayoutResourcePtr();
    }

    QnLayoutResourcePtr resource = layout->resource();
    if(resource.isNull()) {
        qnWarning("Given layout is not registered with workbench synchronizer.");
        return resource;
    }

    return resource;
}

void QnWorkbenchSynchronizer::save(QnWorkbenchLayout *layout, QObject *object, const char *slot) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return;

    /* Submit all changes to the resource. */
    QnWorkbenchLayoutSynchronizer::instance(layout)->submit();

    detail::WorkbenchSynchronizerReplyProcessor *processor = new detail::WorkbenchSynchronizerReplyProcessor(resource);
    connect(processor, SIGNAL(finished(int, const QByteArray &, const QnLayoutResourcePtr &)), object, slot);
    m_connection->saveAsync(m_user, processor, SLOT(at_finished(int, const QByteArray &, QnResourceList, int)));
}

void QnWorkbenchSynchronizer::restore(QnWorkbenchLayout *layout) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return;

    resource->setItems(savedItems(resource).values());
}

bool QnWorkbenchSynchronizer::isChanged(QnWorkbenchLayout *layout) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return false;

    return resource->getItems() != savedItems(resource);
}

bool QnWorkbenchSynchronizer::isLocal(QnWorkbenchLayout *layout) {
    QnLayoutResourcePtr resource = checkLayoutResource(layout);
    if(!resource)
        return false;

    return isCreatedLocally(layout);
}

void QnWorkbenchSynchronizer::start() {
    assert(m_workbench != NULL && !m_user.isNull());

    /* Clean workbench's layouts. */
    while(!m_workbench->layouts().isEmpty())
        delete m_workbench->layouts().back();

    /* Consider all user's layouts saved. */
    foreach(const QnLayoutResourcePtr &resource, m_user->getLayouts())
        if(!hasSavedItems(resource))
            setSavedItems(resource, resource->getItems());

    /* Start listening to changes. */
    connect(m_user.data(),      SIGNAL(resourceChanged()),                  this, SLOT(at_user_resourceChanged()));

    connect(m_workbench,        SIGNAL(aboutToBeDestroyed()),               this, SLOT(at_workbench_aboutToBeDestroyed()));
    connect(m_workbench,        SIGNAL(layoutsChanged()),                   this, SLOT(at_workbench_layoutsChanged()));

    m_submit = m_update = true;

    m_running = true;
    emit started();
}

void QnWorkbenchSynchronizer::stop() {
    assert(m_workbench != NULL && !m_user.isNull());

    emit stopped();
    m_running = false;

    m_submit = m_update = false;

    /* Stop listening to changes. */
    disconnect(m_workbench, NULL, this, NULL);
    disconnect(m_user.data(), NULL, this, NULL);

    /* Clean workbench's layouts. */
    while(!m_workbench->layouts().isEmpty())
        delete m_workbench->layouts().back();
}

void QnWorkbenchSynchronizer::update() {
    if(!m_update)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    QSet<QnLayoutResourcePtr> resources = m_user->getLayouts().toSet();

    /* New layouts may have been added, but these are not on the workbench, 
     * so we don't need to do anything about them. 
     * 
     * Layouts may have been removed, and in this case we need to remove them
     * from the workbench too. */
    foreach(QnWorkbenchLayout *layout, m_workbench->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();

        if(!resources.contains(resource)) /* Corresponding layout resource was removed, remove layout. */
            delete layout;
    }
}

void QnWorkbenchSynchronizer::submit() {
    if(!m_submit)
        return;

    QnScopedValueRollback<bool> guard(&m_update, false);

    /* Layout may have been closed, but it doesn't mean that we should remove it.
     *
     * New layout may have been added, and in this case we need to create a new
     * resource for it. */
    foreach(QnWorkbenchLayout *layout, m_workbench->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();

        if(resource.isNull()) { 
            /* This actually is a newly created layout. */
            resource = QnLayoutResourcePtr(new QnLayoutResource());
            qnResPool->addResource(resource);
            m_user->addLayout(resource);

            setCreatedLocally(layout, true);

            QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(layout, resource, this);
            synchronizer->setAutoDeleting(true);
            synchronizer->submit();
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchSynchronizer::at_user_resourceChanged() {
    update();
}

void QnWorkbenchSynchronizer::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}

void QnWorkbenchSynchronizer::at_workbench_layoutsChanged() {
    submit();
}
