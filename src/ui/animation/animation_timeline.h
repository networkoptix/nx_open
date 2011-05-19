#ifndef cl_animation_timeline_h_0004
#define cl_animation_timeline_h_0004

enum CLAnimationCurve
{
    SLOW_END, // slow end
    SLOW_END_POW_20,
    SLOW_END_POW_22,
    SLOW_END_POW_25,
    SLOW_END_POW_30,
    SLOW_END_POW_35,
    SLOW_END_POW_40,
    SLOW_START_SLOW_END,
    INOUTBACK,
    OUTCUBIC,
    SLOW_START 
};

QEasingCurve& easingCurve(CLAnimationCurve ac);


#endif //cl_animation_timeline_h_0004
