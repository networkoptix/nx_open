#ifndef __THIRD_PARTY_STORAGE_LIBRARY_H__
#define __THIRD_PARTY_STORAGE_LIBRARY_H__
 
#if defined(_WIN32)
    #if defined(STORAGE_EXPORTS)
        #define STORAGE_API __declspec(dllexport)
	#else
		#define STORAGE_API
    #endif
#else
    #define STORAGE_API
#endif
 
namespace Qn
{
    class StorageFactory;
}
 
/**
*   DLL entry point
*  
*   \return     Pointer to Storage class, NULL otherwise.
*/
extern "C" STORAGE_API Qn::StorageFactory* create_qn_storage_factory();
 
/**
*   \return     error message for the given error code.
*/
extern "C" STORAGE_API const char* qn_storage_error_message(int ecode);
 
#endif // __THIRD_PARTY_STORAGE_LIBRARY_H__