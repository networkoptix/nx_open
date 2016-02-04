
#include "helpers.h"

helpers::QnActiveDestructor::QnActiveDestructor(const CallbackType &callback)
    : m_callback(callback)
{
}

helpers::QnActiveDestructor::~QnActiveDestructor()
{
    if (m_callback)
        m_callback();
}
