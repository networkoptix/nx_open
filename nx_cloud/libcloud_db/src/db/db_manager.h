/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_db_manager_h
#define cloud_db_db_manager_h

#include <utils/common/singleton.h>


enum class DBResult
{
    ok,
    ioError
};


//!
class DBTransaction
{
public:
    DBResult commit();
};


/*!
    Scales DB operations on multiple threads
 */
class DBManager
:
    public Singleton<DBManager>
{
public:
    //!Executes \a dbUpdateFunc within proper thread and passes transaction to it
    /*!
        \param dbUpdateFunc This function has to commit changes. If commit does not happen, rollback will be called
        \param completionHandler Result returned by \a dbUpdateFunc is passed here
    */
    void executeUpdate( std::function<DBResult(DBTransaction&)> dbUpdateFunc );
    void executeSelect( std::function<DBResult()> dbSelectFunc );
};

#endif
