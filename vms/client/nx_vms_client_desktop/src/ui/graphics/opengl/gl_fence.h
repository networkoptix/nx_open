/**********************************************************
* 23 jan 2013
* a.kolesnikov
***********************************************************/

#ifndef NX_GLFENCE_H
#define NX_GLFENCE_H

#include <QtGui/QOpenGLFunctions>

//class QOpenGLFunctions_3_2_Core;

//!Incapsulates OpenGL sync object, defined in ARB_sync extension
/*!
    Method sync() blocks until all OGL operations, submitted before \a placeFence() call, have been processed.
    This object is a good alternative to "glFlush(); glFinish();" sequence

    \note Present on OpenGL >= 3.2
    \note If no ARB_sync extension present, \a placeFence() method does glFlush(); glFinish(); instead of sync object creation. In this case, \a sync() does nothing
    \note Not thread-safe. Methods \a placeFence() and \a sync() can be called from different threads, but MUST be synchronized by caller.
*/
class GLFence: protected QOpenGLFunctions
{
public:
    GLFence();
    ~GLFence();

    //!Creates ARB_sync object
    /*!
        Subsequence \a sync() call will block until all OGL operations, submitted up to this point, have been processed
        \return true, if object has been created successfully (or glFlush(); glFinish() have been called)
    */
    bool placeFence();
    //!Blocks until all OGL operations, submitted up to previous \a placeFence() call, have been processed
    void sync();
    bool trySync();

private:
 /*   QOpenGLFunctions_3_2_Core* versionFunctions();*/
    bool arbSyncPresent();

    void* m_fenceSyncName;
 //   QOpenGLFunctions_3_2_Core* m_versionFunctions;
    bool m_versionFunctionsInitialized;
};

#endif  //NX_GLFENCE_H
