#include "resource_search_synchronizer.h"
#include <cassert>
#include <ui/workbench/workbench_layout.h>
#include "resource_search_proxy_model.h"

Q_DECLARE_METATYPE(QnResourceSearchSynchronizer *);

class QnResourceSearchSynchronizerCriterion {
public:
    static bool match(const QnResourcePtr &resource, const QVariant &targetValue) {
        QnResourceSearchSynchronizer *synchronizer = targetValue.value<QnResourceSearchSynchronizer *>();
        if(synchronizer == NULL)
            return true;

        if(synchronizer->m_manuallyAdded.contains(resource->getUniqueId()))
            return true;

        return false;
    }
};

QnResourceSearchSynchronizer::QnResourceSearchSynchronizer(QObject *parent):
    m_layout(NULL),
    m_model(NULL),
    m_submit(false),
    m_update(false)
{}

QnResourceSearchSynchronizer::~QnResourceSearchSynchronizer() {
    setModel(NULL);
    setLayout(NULL);
}

void QnResourceSearchSynchronizer::setModel(QnResourceSearchProxyModel *model) {
    if(m_model == model)
        return;

    if(m_model != NULL && m_layout != NULL)
        stop();

    m_model = model;

    if(m_model != NULL && m_layout != NULL)
        start();
}

void QnResourceSearchSynchronizer::setLayout(QnWorkbenchLayout *layout) {
    if(m_layout == layout)
        return;

    if(m_model != NULL && m_layout != NULL)
        stop();

    m_layout = layout;

    if(m_model != NULL && m_layout != NULL)
        start();
}

void QnResourceSearchSynchronizer::start() {
    assert(m_model != NULL && m_layout != NULL);

    //m_layout->ite


    QnResourceCriterionGroup criterionGroup(QnResourceCriterionGroup::ANY_GROUP);
    criterionGroup.addCriterion(QnResourceCriterion(&QnResourceSearchSynchronizerCriterion::match, QVariant::fromValue<QnResourceSearchSynchronizer *>(this)));
    criterionGroup.addCriterion(m_model->criterion());

    //m_criterion = QnResourceCriterionGroup(QnResourceCriterionGroup::ANY_GROUP);

    connect(m_layout,   SIGNAL(aboutToBeDestroyed()), this, SLOT(at_layout_aboutToBeDestroyed()));
    connect(m_model,    SIGNAL(destroyed(QObject *)), this, SLOT(at_model_destroyed()));

    m_submit = m_update = true;
}


void QnResourceSearchSynchronizer::stop() {
    assert(m_model != NULL && m_layout != NULL);


    m_submit = m_update = false;
}

void QnResourceSearchSynchronizer::at_layout_aboutToBeDestroyed() {
    setLayout(NULL);
}

void QnResourceSearchSynchronizer::at_model_destroyed() {
    setModel(NULL);
}

