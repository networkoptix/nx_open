
#include "systems_finder.h"

#include <network/direct_systems_finder.h>

QnSystemsFinder::QnSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_directSystemsFinder(new QnDirectSystemsFinder())
    , m_cloudSystemsFinder(/*new QnCloudSystemsFinder()*/)
{

}

QnSystemsFinder::~QnSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnSystemsFinder::systems() const
{
    return (/* m_cloudSystemsFinder->systems() + */ m_directSystemsFinder->systems());
}
