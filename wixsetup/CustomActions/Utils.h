#ifndef _CUSTOM_ACTIONS_UTILS_H_
#define _CUSTOM_ACTIONS_UTILS_H_

LPCWSTR GetProperty(MSIHANDLE hInstall, LPCWSTR name);

CString GenerateGuid();

bool IsPortAvailable(int port);
bool IsPortRangeAvailable(int firstPort, int count);

void InitWinsock();
void FinishWinsock();

#endif // _CUSTOM_ACTIONS_UTILS_H_
