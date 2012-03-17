#ifndef _CUSTOM_ACTIONS_UTILS_H_
#define _CUSTOM_ACTIONS_UTILS_H_

LPCWSTR GetProperty(MSIHANDLE hInstall, LPCWSTR name);

bool IsPortAvailable(int port);
int NextFreePort(int startPort, int endPort);

void InitWinsock();
void FinishWinsock();

#endif // _CUSTOM_ACTIONS_UTILS_H_
