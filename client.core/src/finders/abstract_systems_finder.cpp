
#include "abstract_systems_finder.h"

QnAbstractSystemsFinder::QnAbstractSystemsFinder(QObject *parent)
    : base_type(parent)
{}

QnAbstractSystemsFinder::~QnAbstractSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnAbstractSystemsFinder::systems() const
{
    return SystemDescriptionList();
}


