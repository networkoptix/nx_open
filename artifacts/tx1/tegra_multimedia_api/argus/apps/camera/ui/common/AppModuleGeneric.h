/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ARGUS_APPS_CAMERA_UI_COMMON_APP_MODULE_GENERIC_H
#define ARGUS_APPS_CAMERA_UI_COMMON_APP_MODULE_GENERIC_H

#include "IAppModule.h"
#include "Window.h"

namespace ArgusSamples
{

/**
 * The base app module provides basic settings common to all app modules
 */
class AppModuleGeneric : public IAppModule
{
public:
    AppModuleGeneric();
    virtual ~AppModuleGeneric();

    /** @name IAppModule methods */
    /**@{*/
    virtual bool initialize(Options &options);
    virtual bool shutdown();
    virtual bool start(Window::IGuiMenuBar *iGuiMenuBar = NULL,
        Window::IGuiContainer *iGuiContainerConfig = NULL);
    virtual bool stop();
    /**@}*/

    /** @name option callbacks */
    /**@{*/
    static bool info(void *userPtr, const char *optArg);
    static bool loadConfig(void *userPtr, const char *optArg);
    static bool saveConfig(void *userPtr, const char *optArg);
    static bool quit(void *userPtr, const char *optArg);
    static bool verbose(void *userPtr, const char *optArg);
    static bool kpi(void *userPtr, const char *optArg);
    static bool device(void *userPtr, const char *optArg);
    static bool exposureTimeRange(void *userPtr, const char *optArg);
    static bool focusPosition(void *userPtr, const char *optArg);
    static bool gainRange(void *userPtr, const char *optArg);
    static bool sensorMode(void *userPtr, const char *optArg);
    static bool frameRate(void *userPtr, const char *optArg);
    static bool outputSize(void *userPtr, const char *optArg);
    static bool outputPath(void *userPtr, const char *optArg);
    static bool autoFocus(void *userPtr, const char *optArg);
    static bool autoExposure(void *userPtr, const char *optArg);
    static bool vstab(void *userPtr, const char *optArg);
    static bool deNoise(void *userPtr, const char *optArg);
    static bool aeAntibanding(void *userPtr, const char *optArg);
    static bool aeLock(void *userPtr, const char *optArg);
    static bool awbLock(void *userPtr, const char *optArg);
    static bool awb(void *userPtr, const char *optArg);
    static bool exposureCompensation(void *userPtr, const char *optArg);
    static bool deFogEnable(void *userPtr, const char *optArg);
    static bool deFogAmount(void *userPtr, const char *optArg);
    static bool deFogQuality(void *userPtr, const char *optArg);
    /**@}*/

private:
    bool m_initialized;                 ///< set if initialized
    bool m_running;                     ///< set if running
    Window::IGuiMenuBar *m_guiMenuBar;  ///< menu bar
    Window::IGuiContainer *m_guiContainerConfig; ///< configuration GUI container
    Window::IGuiContainerGrid *m_guiConfig; ///< configuration GUI
};

}; // namespace ArgusSamples

#endif // ARGUS_APPS_CAMERA_UI_COMMON_APP_MODULE_GENERIC_H
