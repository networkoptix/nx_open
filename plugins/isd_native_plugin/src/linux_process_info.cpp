/**********************************************************
* 17 jun 2014
* akolesnikov@networkoptix.com
***********************************************************/

#include "linux_process_info.h"


namespace ProcessState
{
    /*!
        \param strSize If -1, \a str MUST be NULL-terminated
    */
    Value fromString( const char* str, ssize_t strSize )
    {
        if( strSize == 0 )
            return unknown;
        switch( str[0] )
        {
            case 'R':
                if( strncmp( str, "R (running)", (size_t)strSize ) == 0 )
                    return running;
                return unknown;
            case 'S':
                if( strncmp( str, "S (sleeping)", (size_t)strSize ) == 0 )
                    return sleeping;
                return unknown;
            case 'D':
                if( strncmp( str, "D (disk sleep)", (size_t)strSize ) == 0 )
                    return diskSleep;
                return unknown;
            case 'T':
                if( strncmp( str, "T (stopped)", (size_t)strSize ) == 0 )
                    return stopped;
                if( strncmp( str, "T (tracing stop)", (size_t)strSize ) == 0 )
                    return tracingStop;
                return unknown;
            case 'Z':
                if( strncmp( str, "Z (zombie)", (size_t)strSize ) == 0 )
                    return zombie;
                return unknown;
            case 'X':
                if( strncmp( str, "X (dead)", (size_t)strSize ) == 0 )
                    return dead;
                return unknown;
            default:
                return unknown;
        }
    }

    const char* toString( Value val )
    {
        switch( val )
        {
            case running:
                return "R (running)";
            case sleeping:
                return "S (sleeping)";
            case diskSleep:
                return "D (disk sleep)";
            case stopped:
                return "T (stopped)";
            case tracingStop:
                return "T (tracing stop)";
            case zombie:
                return "Z (zombie)";
            case dead:
                return "X (dead)";
            default:
                return "unknown";
        }
    }
}

static void trimmInPlace( char** str )
{
    while( **str == ' ' || **str == '\n' || **str == '\r' )
        ++(*str);
    size_t len = strlen( *str );
    while( len > 0 )
    {
        const char ch = (*str)[len-1];
        if( ch == ' ' || ch == '\n' || ch == '\r' )
        {
            (*str)[len-1] = '\0';
            --len;
        }
        else
        {
            break;
        }
    }
}

static const char* skipStartingSpaces( const char* str )
{
    const char* resultStr = str;
    while( *resultStr == ' ' || *resultStr == '\n' || *resultStr == '\r' || *resultStr == 9 )
        ++resultStr;
    return resultStr;
}

static size_t readSizeInBytes( const char* str )
{
    size_t result = 0;
    const char* numberStart = skipStartingSpaces(str);
    const char* numberEnd = numberStart;
    for( ;; ++numberEnd )
    {
        const char ch = *numberEnd;
        if( !(ch >= '0' && ch <= '9') )
            break;
        result *= 10;
        result += ch - '0';
    }

    const char* dimensionStart = skipStartingSpaces(numberEnd);
    if( *dimensionStart == '\0' )
        return result;

    switch( *dimensionStart )
    {
        case 'k':
        case 'K':
            result <<= 10;
            break;
        case 'm':
        case 'M':
            result <<= 20;
            break;
        case 'g':
        case 'G':
            result <<= 30;
            break;
    }

    return result;
}

bool readProcessStatus( ProcessStatus* const processStatus )
{
    FILE* statusFile = fopen( "/proc/self/status", "r" );
    if( statusFile == NULL )
        return false;
    ssize_t bytesRead = -1;
    size_t lineLen = 0;
    char* line = nullptr;
    while( (bytesRead = getline( &line, &lineLen, statusFile )) != -1 )
    {
        if( bytesRead == 0 )
            break;  //end of file
        trimmInPlace( &line );  //removing spaces from beginning and ending
        const char* sepPos = strchr( line, ':' );
        if( sepPos == NULL )
            continue;   //invalid format
        switch( line[0] )
        {
            case 'V':
                if( strncmp( line, "VmPeak", sepPos-line ) == 0 )
                    processStatus->vmPeak = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmSize",  sepPos-line ) == 0 )
                    processStatus->vmSize = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmLck", sepPos-line ) == 0 )
                    processStatus->vmLck = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmHWM", sepPos-line ) == 0 )
                    processStatus->vmHWM = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmRSS", sepPos-line ) == 0 )
                    processStatus->vmRSS = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmData", sepPos-line ) == 0 )
                    processStatus->vmData = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmStk", sepPos-line ) == 0 )
                    processStatus->vmStk = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmExe", sepPos-line ) == 0 )
                    processStatus->vmExe = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmLib", sepPos-line ) == 0 )
                    processStatus->vmLib = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                else if( strncmp( line, "VmPTE", sepPos-line ) == 0 )
                    processStatus->vmPTE = readSizeInBytes( skipStartingSpaces(sepPos+1) );
                break;

            case 'S':
                if( strncmp( line, "State", sepPos-line ) == 0 )
                    processStatus->state = ProcessState::fromString( skipStartingSpaces(sepPos+1) );
                break;
        }
    }

    if( line )
        free( line );

    fclose( statusFile );
    return true;
}
