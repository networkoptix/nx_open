#include <stdio.h>

#include "../../src/plugin_api.h"
#include "library_loader.h"

using CreateNXPluginInstanceType = nxpl::PluginInterface* (*)();

int main(int argc, const char* argv[])
{
    Library lib("plugins/libteststorageplugin.so");
    printf("Lib is opened: %d\n", lib.isOpened());

    auto instanceFunc = lib.symbol<CreateNXPluginInstanceType>("createNXPluginInstance");
}