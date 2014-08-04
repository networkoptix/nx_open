#ifndef _CUSTOM_ACTIONS_UTILS_H_
#define _CUSTOM_ACTIONS_UTILS_H_

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

#endif // _CUSTOM_ACTIONS_UTILS_H_
