#ifndef _EVEASSOC_CA_ERRORS_H_
#define _EVEASSOC_CA_ERRORS_H_

typedef std::exception Exception;

class Error : public Exception
{
public:
    Error(const char* description)
        : Exception(description)
    {
    }

};

class InitializationError : public Error
{
public:
    InitializationError(const char* description)
        : Error(description)
    {
    }
};

#endif // _EVEASSOC_CA_ERRORS_H_