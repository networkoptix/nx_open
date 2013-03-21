////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "custom_transition.h"


Conditional::~Conditional()
{
    for( std::vector<AbstractCondition*>::size_type
        i = 0;
        i < m_conditions.size();
        ++i )
    {
        delete m_conditions[i];
    }
    m_conditions.clear();
}

bool Conditional::checkCondition() const
{
    for( std::vector<AbstractCondition*>::size_type
        i = 0;
        i < m_conditions.size();
        ++i )
    {
        if( !m_conditions[i]->checkCondition() )
            return false;
    }
    return true;
}

void Conditional::addCondition( AbstractCondition* condition )
{
    m_conditions.push_back( condition );
}


ConditionalSignalTransition::ConditionalSignalTransition(
    QObject* const sender,
    const char* signal,
    QState* const sourceState,
    QAbstractState* const targetState,
    AbstractCondition* condition )
:
    QSignalTransition(
        sender,
        signal,
        sourceState )
{
    setTargetState( targetState );
    if( condition )
        addCondition( condition );
}

bool ConditionalSignalTransition::eventTest( QEvent* _event )
{
    if( !QSignalTransition::eventTest( _event ) )
        return false;
    return checkCondition();
}
