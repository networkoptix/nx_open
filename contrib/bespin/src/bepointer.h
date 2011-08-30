#if 0 // QT_VERSION >= 0x040600
    #warning using new weak pointer!
    #include <QWeakPointer>
    #define BePointer QWeakPointer
#else
    #include <QPointer>
    #define BePointer QPointer
#endif
