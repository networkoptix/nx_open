/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_db_manager_h
#define cloud_db_db_manager_h

#include <utils/common/singleton.h>


namespace cdb {
namespace db {


enum class DBResult
{
    ok,
    ioError
};


//!Transaction
class DBTransaction
{
public:
    DBResult commit();
};

//!Executes DB request
/*!
    Scales DB operations on multiple threads
 */
class DBManager
:
    public Singleton<DBManager>
{
public:
    //!Executes data modification request that spawns some output data
    /*!
        Hold multiple threads inside. \a dbUpdateFunc is executed within random thread.
        \param dbUpdateFunc This function may executed SQL commands and fill output data
        \param completionHandler DB operation result is passed here
        \note DB operation may fail even if \a dbUpdateFunc finished successfully (e.g., transaction commit fails)
    */
    template<typename InputData, typename OutputData>
    void executeUpdate(
        std::function<DBResult(DBTransaction&, const InputData&, OutputData* const)> dbUpdateFunc,
        InputData&& input,
        std::function<void(DBResult, InputData&&, OutputData&&)> completionHandler );
    //!Overload for updates with no output data
    template<typename InputData>
    void executeUpdate(
        std::function<DBResult(DBTransaction&, const InputData&)> dbUpdateFunc,
        InputData&& input,
        std::function<void(DBResult, InputData&&)> completionHandler );
    template<typename OutputData>
    void executeSelect( std::function<DBResult(OutputData* const)> dbSelectFunc );
};


}   //db
}   //cdb

#endif
