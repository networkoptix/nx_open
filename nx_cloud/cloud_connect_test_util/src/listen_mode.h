/**********************************************************
* mar 29, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <iostream>
#include <map>

#include <QString>


int runInListenMode(const std::multimap<QString, QString>& args);
void printListenOptions(std::ostream* const outStream);
