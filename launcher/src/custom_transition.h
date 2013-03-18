////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef CUSTOM_TRANSITION_H
#define CUSTOM_TRANSITION_H

#include <functional>
#include <vector>

#include <QVariant>
#include <QSignalTransition>


class AbstractCondition
{
public:
    virtual ~AbstractCondition() {}

    virtual bool checkCondition() const = 0;
};

template<class T, class Pred>
class ObjectPropertyCheckCondition
:
    public AbstractCondition
{
public:
    ObjectPropertyCheckCondition(
        const QObject* const obj,
        const char* propertyName,
        const T& valueToCheck )
    :
        m_obj( obj ),
        m_propertyName( propertyName ),
        m_valueToCheck( valueToCheck )
    {
    }

    //!Returns result of applying binary predicate \a Pred to current property value and value given at object instanciation
    virtual bool checkCondition() const
    {
        return Pred()( m_obj->property(m_propertyName.c_str()).value<T>(), m_valueToCheck );
    }

private:
    const QObject* const m_obj;
    std::string m_propertyName;
    T m_valueToCheck;
};

template<typename T>
struct ObjectPropertyEqualConditionHelper
{
    typedef ObjectPropertyCheckCondition<T, std::equal_to<T> > CondType;
};

template<typename T>
struct ObjectPropertyGreaterConditionHelper
{
    typedef ObjectPropertyCheckCondition<T, std::greater<T> > CondType;
};

template<typename T>
struct ObjectPropertyLessConditionHelper
{
    typedef ObjectPropertyCheckCondition<T, std::less<T> > CondType;
};


class Conditional
:
    public AbstractCondition
{
public:
    virtual ~Conditional();

    //!Implementation of AbstractCondition::checkCondition
    virtual bool checkCondition() const;
    /*!
        Multiple conditions can be added.
        Takes ownership of \a condition
    */
    void addCondition( AbstractCondition* condition );

private:
    std::vector<AbstractCondition*> m_conditions;
};

class ConditionalSignalTransition
:
    public QSignalTransition,
    public Conditional
{
public:
    /*!
        Takes ownership of \a condition
    */
    ConditionalSignalTransition(
        QObject* const sender,
        const char* signal,
        QState* const sourceState,
        QAbstractState* const targetState,
        AbstractCondition* condition = NULL );

protected:
    virtual bool eventTest( QEvent* _event );
};

#endif  //CUSTOM_TRANSITION_H
