#ifndef ITE_RET_CHANNEL_COM
#define ITE_RET_CHANNEL_COM

#include <termios.h>

namespace ite
{
    class ComPort
    {
    public:
        static ComPort * Instance;

        ComPort(const char * devName, unsigned baudrate);
        ~ComPort();

        bool isOK() const { return cport_ >= 0; }

        int poll(unsigned char * buf, int size);
        int send(const unsigned char * buf, int size);

    private:
        int cport_;
        int baudrate_;
        const char * name_;
        struct termios old_port_settings;
        struct termios new_port_settings;

        static int checkBaudrate(unsigned baudrate);
    };
}

#endif
