
#pragma once

namespace helpers
{
    class QnActiveDestructor
    {
    public:
        typedef std::function<void ()> CallbackType;
        QnActiveDestructor(const CallbackType &callback);

        ~QnActiveDestructor();

    private:
        const CallbackType m_callback;
    };
}