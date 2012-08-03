#ifndef cl_rand_12_06
#define cl_rand_12_06

#include <time.h>

static unsigned int cl_get_random_val(unsigned int minVal, unsigned int maxVal)
{
    static bool first = true;
    if (first)
    {
        srand((unsigned)time(0)); 
        first = false;
    }

    return minVal + rand()%(maxVal - minVal);

}

#endif //cl_rand_12_06
