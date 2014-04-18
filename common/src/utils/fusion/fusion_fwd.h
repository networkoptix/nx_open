#ifndef QN_FUSION_FWD_H
#define QN_FUSION_FWD_H

/**
 * 
 */
#define QN_FUSION_ENABLE_PRIVATE()                                              \
    template<class T>                                                           \
    friend struct QnFusionBinding;


#endif // QN_FUSION_FWD_H
