
#include "systems_finder.h"

#include <network/direct_systems_finder.h>

QnSystemsFinder::QnSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_directSystemsFinder(new QnDirectSystemsFinder())
    , m_cloudSystemsFinder(/*new QnCloudSystemsFinder()*/)
{
    const auto connectFinder = [this](const SystemsFinderPtr &finder)
    {
        connect(finder, &QnAbstractSystemsFinder::systemDiscovered
            , this, &QnSystemsFinder::systemDiscovered);
        connect(finder, &QnAbstractSystemsFinder::systemLost
            , this, &QnSystemsFinder::systemLost);
    };

    connectFinder(m_directSystemsFinder);
}

QnSystemsFinder::~QnSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnSystemsFinder::systems() const
{
    return (/* m_cloudSystemsFinder->systems() + */ m_directSystemsFinder->systems());
}
