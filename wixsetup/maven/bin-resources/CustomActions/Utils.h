#ifndef _CUSTOM_ACTIONS_UTILS_H_
#define _CUSTOM_ACTIONS_UTILS_H_

class Error {
public:
    Error(LPCWSTR msg);

    LPCWSTR msg() const;

private:
    CAtlString _msg;
};

LPCWSTR GetProperty(MSIHANDLE hInstall, LPCWSTR name);

CString GenerateGuid();

bool IsPortAvailable(int port);
bool IsPortRangeAvailable(int firstPort, int count);

void InitWinsock();
void FinishWinsock();

int CopyDirectory(const CAtlString &refcstrSourceDirectory,
                  const CAtlString &refcstrDestinationDirectory);


UINT CopyProfile(MSIHANDLE hInstall, const char* actionName, BOOL verifyDestFolderExists = TRUE);
void fixPath(CString& path);

void QuitExecAndWarn(const LPWSTR commandLine, int status, const LPWSTR warningMsg);
bool isStandaloneSystem(const char* host);

CString GetAppDataLocalFolderPath();

#endif // _CUSTOM_ACTIONS_UTILS_H_
