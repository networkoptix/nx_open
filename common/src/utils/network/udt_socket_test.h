#ifndef __UDT_SOCKET_TEST_H__
#define __UDT_SOCKET_TEST_H__
#include <memory>

// Usage:
// Setup a project that links to the common.lib ( including all the other environments )
// And in the main function puts code like this:
// 
// int main( int argc , char** argv ) {
//      QCoreApplication app(argc,argv);
//      std::unique_ptr<UdtSocketProfile> test = createUdtSocketProfile(app);
//      if( test ) {
//          test->run();
//      }
//      return 0;
// }
// 
// Based on the command line parameter, you could specify the running instance is a server
// or a client. 
// Command line parameter:
// --s    : Set it up as an server, if --s is not there, it is a client
// --addr : The IPV4 address it gonna use, for a server, it is the bind address; a client, it is the connect address
// --port : Port number
// --thread_size: The worker thread pool size
// 
// Client specific parameters
// 
// --active_rate : A value resides in [0,1.0] to represents the amount of active client, the active client will send random bytes to server
// --disconn_rate: A value resides in [0,1.0] to represents a specific server disconnection rates possibility.
// --max_conn: A value represents the maximum concurrent connection client
// --min_content_len: A value represents the minimum byte length of random byte content. If not present , a default value will be used
// --max_content_len: A value represents the maximum byte length of random byte content. If not present , a default value will be used

// Notes:
// For analyzing, only very simple statistic will be output on the screen. So, some third party analyzing program should be used , like netstat.
// I have no idea for the analyzing right now, and I will continue exploring on this thing. 

class UdtSocketProfile {
public:
    virtual bool run() = 0;
    virtual void quit() =0;
    virtual ~UdtSocketProfile() {}
};

class QCoreApplication;
std::unique_ptr<UdtSocketProfile> createUdtSocketProfile( const QCoreApplication& app );

#endif // __UDT_SOCKET_TEST_H__