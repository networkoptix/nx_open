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

#include <stdlib.h>
#include <string.h>

#include "AppModuleGeneric.h"
#include "XMLConfig.h"
#include "Dispatcher.h"
#include "Error.h"
#include "Options.h"
#include "Window.h"

#include <Argus/Ext/DeFog.h>

namespace ArgusSamples
{

/**
 * Default configuration file name
 */
#define DEFAULT_CONFIG_FILE "argusAppConfig.xml"

/* static */ bool AppModuleGeneric::info(void *userPtr, const char *optArg)
{
    std::string info;
    PROPAGATE_ERROR(Dispatcher::getInstance().getInfo(info));
    printf("%s\n", info.c_str());

    return true;
}

/* static */ bool AppModuleGeneric::loadConfig(void *userPtr, const char *optArg)
{
    /// @todo ask for file if called from GUI

    const char *configFile = DEFAULT_CONFIG_FILE;
    if (optArg)
        configFile = optArg;
    PROPAGATE_ERROR(ArgusSamples::loadConfig(configFile));
    return true;
}

/* static */ bool AppModuleGeneric::saveConfig(void *userPtr, const char *optArg)
{
    /// @todo ask for file if called from GUI

    const char *configFile = DEFAULT_CONFIG_FILE;
    if (optArg)
        configFile = optArg;
    PROPAGATE_ERROR(ArgusSamples::saveConfig(configFile));
    return true;
}

/* static */ bool AppModuleGeneric::quit(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Window::getInstance().requestExit());
    return true;
}

/* static */ bool AppModuleGeneric::verbose(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_verbose.set(true));
    return true;
}

/* static */ bool AppModuleGeneric::kpi(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_kpi.set(true));
    return true;
}

/* static */ bool AppModuleGeneric::device(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_deviceIndex.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::exposureTimeRange(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_exposureTimeRange.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::focusPosition(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_focusPosition.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::gainRange(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_gainRange.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::sensorMode(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_sensorModeIndex.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::frameRate(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_frameRate.setFromString(optArg));

    return true;
}

/* static */ bool AppModuleGeneric::outputSize(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_outputSize.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::outputPath(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_outputPath.set(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::vstab(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_vstabMode.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::deNoise(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_denoiseMode.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::aeAntibanding(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_aeAntibandingMode.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::aeLock(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_aeLock.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::awbLock(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_awbLock.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::awb(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_awbMode.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::exposureCompensation(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_exposureCompensation.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::deFogEnable(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_deFogEnable.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::deFogAmount(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_deFogAmount.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleGeneric::deFogQuality(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_deFogQuality.setFromString(optArg));
    return true;
}

AppModuleGeneric::AppModuleGeneric()
    : m_initialized(false)
    , m_running(false)
    , m_guiMenuBar(NULL)
    , m_guiContainerConfig(NULL)
    , m_guiConfig(NULL)
{
}

AppModuleGeneric::~AppModuleGeneric()
{
    shutdown();
}

bool AppModuleGeneric::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(options.addDescription(
        "The supported value range of some settings is device or sensor mode dependent.\n"
        "Use the '--info' option to get a list of the supported values.\n"));

    PROPAGATE_ERROR(options.addOption(
        Options::Option("info", 'i', "",
            Options::Option::TYPE_ACTION, Options::Option::FLAG_NO_ARGUMENT,
            "print information on devices.", info)));

    PROPAGATE_ERROR(options.addOption(
        Options::Option("loadconfig", 0, "FILE",
            Options::Option::TYPE_ACTION, Options::Option::FLAG_OPTIONAL_ARGUMENT,
            "load configuration from XML FILE. Default for file is '" DEFAULT_CONFIG_FILE "'.",
            loadConfig)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("saveconfig", 0, "FILE",
            Options::Option::TYPE_ACTION, Options::Option::FLAG_OPTIONAL_ARGUMENT,
            "save configuration to XML FILE. Default for file is '" DEFAULT_CONFIG_FILE "'.",
            saveConfig)));

    PROPAGATE_ERROR(options.addOption(
        Options::Option("verbose", 0, "", Dispatcher::getInstance().m_verbose,
            "enable verbose mode.", verbose)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("kpi", 0, "", Dispatcher::getInstance().m_kpi,
            "enable kpi mode.", kpi)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("device", 'd', "INDEX", Dispatcher::getInstance().m_deviceIndex,
            "select camera device with INDEX.", device)));

    // source settings
    PROPAGATE_ERROR(options.addOption(
        Options::Option("exposuretimerange", 0, "RANGE",
            Dispatcher::getInstance().m_exposureTimeRange,
            "sets the exposure time range to RANGE, in nanoseconds.",
            exposureTimeRange)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("gainrange", 0, "RANGE", Dispatcher::getInstance().m_gainRange,
            "sets the gain range to RANGE.", gainRange)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("sensormode", 0, "INDEX", Dispatcher::getInstance().m_sensorModeIndex,
            "set sensor mode to INDEX.", sensorMode)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("framerate", 0, "RATE", Dispatcher::getInstance().m_frameRate,
            "set the sensor frame rate to RATE. If RATE is 0 then VFR (variable frame rate) is "
            "enabled.", frameRate)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("focusposition", 0, "POSITION", Dispatcher::getInstance().m_focusPosition,
            "sets the focus position to POSITION, in focuser units.",
            focusPosition)));

    // output settings
    PROPAGATE_ERROR(options.addOption(
        Options::Option("outputsize", 0, "WIDTHxHEIGHT", Dispatcher::getInstance().m_outputSize,
            "set the still and video output size to WIDTHxHEIGHT (e.g. 1920x1080). If WIDTHxHEIGHT "
            "is '0x0' the output size is the sensor mode size.", outputSize)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("outputpath", 0, "PATH", Dispatcher::getInstance().m_outputPath,
            "set the output file path. A file name, an incrementing index and the file extension will "
            "be appended. E.g. setting 'folder/' will result in 'folder/image0.jpg' or "
            "'folder/video0.mp4'. '/dev/null' can be used to discard output.",
            outputPath)));

    // stream settings
    PROPAGATE_ERROR(options.addOption(
        Options::Option("vstab", 0, "MODE", Dispatcher::getInstance().m_vstabMode,
            "set the video stabilization mode.", vstab)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("denoise", 0, "MODE", Dispatcher::getInstance().m_denoiseMode,
            "set the denoising mode.", deNoise)));

        // auto control settings
    PROPAGATE_ERROR(options.addOption(
        Options::Option("aeantibanding", 0, "MODE", Dispatcher::getInstance().m_aeAntibandingMode,
            "set the auto exposure antibanding mode.", aeAntibanding)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("aelock", 0, "LOCK", Dispatcher::getInstance().m_aeLock,
            "set the auto exposure lock.", aeLock)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("awblock", 0, "LOCK", Dispatcher::getInstance().m_awbLock,
            "set the auto white balance lock.", awbLock)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("awb", 0, "MODE", Dispatcher::getInstance().m_awbMode,
            "set the auto white balance mode.", awb)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("exposurecompensation", 0, "COMPENSATION",
            Dispatcher::getInstance().m_exposureCompensation,
            "set the exposure compensation to COMPENSATION.",
            exposureCompensation)));

    if (Dispatcher::getInstance().supportsExtension(Argus::EXT_DE_FOG))
    {
        // DeFog extension settings
        PROPAGATE_ERROR(options.addOption(
            Options::Option("defog", 0, "ENABLE", Dispatcher::getInstance().m_deFogEnable,
                "set the DeFog enable flag to ENABLE.", deFogEnable)));
        PROPAGATE_ERROR(options.addOption(
            Options::Option("defogamount", 0, "AMOUNT", Dispatcher::getInstance().m_deFogAmount,
                "sets the amount of fog to be removed to AMOUNT.", deFogAmount)));
        PROPAGATE_ERROR(options.addOption(
            Options::Option("defogquality", 0, "QUALITY", Dispatcher::getInstance().m_deFogQuality,
                "sets the quality of the DeFog effect to QUALITY.",
                deFogQuality)));
    }

    m_initialized = true;

    return true;
}

bool AppModuleGeneric::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    delete m_guiConfig;
    m_guiConfig = NULL;

    m_initialized = false;

    return true;
}

bool AppModuleGeneric::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    if (m_running)
        return true;

    if (iGuiMenuBar && !m_guiMenuBar)
    {
        // initialize the menu

        UniquePointer<Window::IGuiMenu> menu;
        UniquePointer<Window::IGuiMenuItem> item;

        // create the elements
        Window::IGuiMenu *fileMenu = iGuiMenuBar->getMenu("File");
        if (!fileMenu)
        {
            PROPAGATE_ERROR(Window::IGuiMenu::create("File", &menu));
            PROPAGATE_ERROR(iGuiMenuBar->add(menu.get()));
            fileMenu = menu.get();
            menu.release();
        }
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Load config", AppModuleGeneric::loadConfig,
            NULL, &item));
        PROPAGATE_ERROR(fileMenu->add(item.get()));
        item.release();
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Save Config", AppModuleGeneric::saveConfig,
            NULL, &item));
        PROPAGATE_ERROR(fileMenu->add(item.get()));
        item.release();
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Quit", AppModuleGeneric::quit, NULL,
            &item));
        PROPAGATE_ERROR(fileMenu->add(item.get()));
        item.release();

        Window::IGuiMenu *helpMenu = iGuiMenuBar->getMenu("Help");
        if (!helpMenu)
        {
            PROPAGATE_ERROR(Window::IGuiMenu::create("Help", &menu));
            PROPAGATE_ERROR(iGuiMenuBar->add(menu.get()));
            helpMenu = menu.get();
            menu.release();
        }
        PROPAGATE_ERROR(Window::IGuiMenuItem::create("Info", AppModuleGeneric::info, NULL,
            &item));
        PROPAGATE_ERROR(helpMenu->add(item.get()));
        item.release();

        m_guiMenuBar = iGuiMenuBar;
    }

    if (iGuiContainerConfig && !m_guiContainerConfig)
    {
        // initialize the GUI

        // create a grid container
        PROPAGATE_ERROR(Window::IGuiContainerGrid::create(&m_guiConfig));

        // create the elements
        UniquePointer<Window::IGuiElement> element;
        Dispatcher &dispatcher = Dispatcher::getInstance();

        Window::IGuiContainerGrid::BuildHelper buildHelper(m_guiConfig);

#define CREATE_GUI_ELEMENT(_NAME, _VALUE)                                           \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(&dispatcher._VALUE, &element));\
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

#define CREATE_GUI_ELEMENT_COMBO_BOX(_NAME, _VALUE, _FROMTYPE, _TOTYPE)             \
        assert(sizeof(_FROMTYPE) == sizeof(_TOTYPE));                               \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(reinterpret_cast<          \
            Value<_TOTYPE>*>(&dispatcher._VALUE), &element));                       \
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

#define CREATE_GUI_ELEMENT_PATH_CHOOSER(_NAME, _VALUE)                              \
        PROPAGATE_ERROR(Window::IGuiElement::createFileChooser(&dispatcher._VALUE,  \
            true, &element));                                                       \
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

        CREATE_GUI_ELEMENT("Verbose", m_verbose);
        CREATE_GUI_ELEMENT("KPI", m_kpi);
        CREATE_GUI_ELEMENT("Device", m_deviceIndex);

        CREATE_GUI_ELEMENT("Exposure time range (ns)", m_exposureTimeRange);
        CREATE_GUI_ELEMENT("Gain range", m_gainRange);

        CREATE_GUI_ELEMENT_COMBO_BOX("Sensor mode index", m_sensorModeIndex,
            uint32_t, Window::IGuiElement::ValueTypeEnum);
        CREATE_GUI_ELEMENT("Frame rate", m_frameRate);
        CREATE_GUI_ELEMENT("Focus position", m_focusPosition);
        CREATE_GUI_ELEMENT("Output size", m_outputSize);

        CREATE_GUI_ELEMENT_PATH_CHOOSER("Output path", m_outputPath);

        CREATE_GUI_ELEMENT_COMBO_BOX("Video stabilization mode", m_vstabMode,
            Argus::VideoStabilizationMode, Argus::NamedUUID);
        CREATE_GUI_ELEMENT_COMBO_BOX("De-Noise mode", m_denoiseMode,
            Argus::DenoiseMode, Argus::NamedUUID);

        CREATE_GUI_ELEMENT_COMBO_BOX("AE antibanding mode", m_aeAntibandingMode,
            Argus::AeAntibandingMode, Window::IGuiElement::ValueTypeEnum);

        CREATE_GUI_ELEMENT("AE Lock", m_aeLock);
        CREATE_GUI_ELEMENT("AWB Lock", m_awbLock);
        CREATE_GUI_ELEMENT_COMBO_BOX("AWB mode", m_awbMode,
            Argus::AwbMode, Argus::NamedUUID);
        CREATE_GUI_ELEMENT("Exposure compensation (ev)", m_exposureCompensation);

#undef CREATE_GUI_ELEMENT
#undef CREATE_GUI_ELEMENT_COMBO_BOX
#undef CREATE_GUI_ELEMENT_PATH_CHOOSER

        m_guiContainerConfig = iGuiContainerConfig;
    }

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->add(m_guiConfig));

    m_running = true;

    return true;
}

bool AppModuleGeneric::stop()
{
    if (!m_running)
        return true;

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->remove(m_guiConfig));

    m_running = true;

    return true;
}

}; // namespace ArgusSamples
