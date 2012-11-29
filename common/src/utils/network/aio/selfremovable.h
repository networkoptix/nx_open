/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SELFREMOVABLE_H
#define SELFREMOVABLE_H


//!Inherit this class to enable object to be freed by callback (usefull for aio handler)
/*!
    \note Class methods are not thread-safe
    \note Descendant class should make its destructor private not to allow object to be created on stack
*/
class SelfRemovable
{
public:
    SelfRemovable();

    //!Tells object to remove itself as soon as possible
    /*!
        Object MUST surely be removed after calling this method
    */
    virtual void scheduleForRemoval();

protected:
    bool isScheduledForRemoval() const;

private:
    bool m_scheduledForRemoval;
};

#endif  //SELFREMOVABLE_H
