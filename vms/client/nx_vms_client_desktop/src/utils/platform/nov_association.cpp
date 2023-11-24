// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nov_association.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>

namespace {

QString quoted(const QString& string)
{
    return '"' + string + '"';
}

} // namespace

namespace nx::vms::utils {

bool registerNovFilesAssociation(
    const QString& customizationId,
    const QString& applicationLauncherBinaryPath,
    const QString& applicationBinaryName,
    const QString& applicationName)
{
    // This function is only needed for Windows now.
    if (!NX_ASSERT(nx::build_info::isWindows()))
        return true;

    static const QString kClassesRootBasePath = "HKEY_CLASSES_ROOT";
    static const QString kLocalMachineBasePath = "HKEY_LOCAL_MACHINE";
    static const QString kOldAssociationClassName = "HDW.nov";
    static const QString kAssociationClassName = "NXFile";
    static const QString kNovExtension = ".nov";
    static const QString kDefaultValueKey = ".";
    static const QString kAssociationDescription = QString("%1 Video (%2)")
        .arg(applicationName, kNovExtension);
    static const QString kContextMenuOldId = QString("play.%1").arg(applicationBinaryName);
    static const QString kContextMenuId = QString("play.%1").arg(applicationBinaryName);
    static const QString kContextMenuTitle = QString("Play with %1").arg(applicationName);

    const QString launcherNativePath = QDir::toNativeSeparators(applicationLauncherBinaryPath);

    QSettings classesRootRegistry(kClassesRootBasePath, QSettings::NativeFormat);

    // Check if the migration has already been done.
    classesRootRegistry.beginGroup(kNovExtension);
    if (classesRootRegistry.value(kDefaultValueKey).toString() == kAssociationClassName)
        return true;
    classesRootRegistry.endGroup();

    // Remove old association class group recursively.
    classesRootRegistry.beginGroup(kOldAssociationClassName);
    classesRootRegistry.remove("");
    classesRootRegistry.endGroup();

    classesRootRegistry.beginGroup(kAssociationClassName);
    {
        classesRootRegistry.setValue(kDefaultValueKey, kAssociationDescription);

        classesRootRegistry.beginGroup("DefaultIcon");
        classesRootRegistry.setValue(kDefaultValueKey, launcherNativePath);
        classesRootRegistry.endGroup();

        classesRootRegistry.beginGroup("shell/open/command");
        classesRootRegistry.setValue(
            kDefaultValueKey, quoted(launcherNativePath) + ' ' + quoted("%1"));
        classesRootRegistry.endGroup();
    }
    classesRootRegistry.endGroup();

    classesRootRegistry.beginGroup(kNovExtension);
    classesRootRegistry.setValue(kDefaultValueKey, kAssociationClassName);
    classesRootRegistry.endGroup();

    classesRootRegistry.beginGroup(
        QString("Applications/%1/SupportedTypes").arg(applicationBinaryName));
    classesRootRegistry.setValue(kDefaultValueKey, kNovExtension);
    classesRootRegistry.endGroup();

    QSettings localMachineRegistry(kLocalMachineBasePath, QSettings::NativeFormat);

    localMachineRegistry.beginGroup(
        QString("SOFTWARE/Classes/SystemFileAssociations/%1/shell").arg(kNovExtension));
    {
        // Remove old record.
        localMachineRegistry.beginGroup(kContextMenuOldId);
        localMachineRegistry.remove("");
        localMachineRegistry.endGroup();

        localMachineRegistry.beginGroup(kContextMenuId);
        {
            localMachineRegistry.setValue(kDefaultValueKey, kContextMenuTitle);

            localMachineRegistry.beginGroup("command");
            localMachineRegistry.setValue(
                kDefaultValueKey, quoted(launcherNativePath) + ' ' + quoted("%1"));
            localMachineRegistry.endGroup();
        }
        localMachineRegistry.endGroup();
    }
    localMachineRegistry.endGroup();

    classesRootRegistry.sync();

    // Ensure the group actually exists after sync() call.
    return classesRootRegistry.childGroups().contains(kAssociationClassName);
}

} // namespace nx::vms::utils
