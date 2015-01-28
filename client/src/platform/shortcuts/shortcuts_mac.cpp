#include "shortcuts_mac.h"

QnMacShortcuts::QnMacShortcuts(QObject *parent /*= NULL*/):
    QnPlatformShortcuts(parent)
{

}

/*
bool createShortcut(NSString *pstrSrcFile,
                    NSString *pstrDstPath,
                    NSString *pstrName)
{
    // First of all we need to figure out which process Id the Finder currently has.
    NSWorkspace *pWS = [NSWorkspace sharedWorkspace];
    NSArray *pApps = [pWS launchedApplications];
    bool fFFound = false;
    ProcessSerialNumber psn;
    for (NSDictionary *pDict in pApps)
    {
        if ([[pDict valueForKey:@"NSApplicationBundleIdentifier"]
isEqualToString:@"com.apple.finder"])
                {
                    psn.highLongOfPSN = [[pDict
valueForKey:@"NSApplicationProcessSerialNumberHigh"] intValue];
                    psn.lowLongOfPSN  = [[pDict
valueForKey:@"NSApplicationProcessSerialNumberLow"] intValue];
                    fFFound = true;
                    break;
                }
    }
    if (!fFFound)
        return false;
    // Now the event fun begins. 
    OSErr err = noErr;
    AliasHandle hSrcAlias = 0;
    AliasHandle hDstAlias = 0;
    do
    {
        // Create a descriptor which contains the target psn. 
        NSAppleEventDescriptor *finderPSNDesc = [NSAppleEventDescriptor
descriptorWithDescriptorType:typeProcessSerialNumber
bytes:&psn
length:sizeof(psn)];
        if (!finderPSNDesc)
            break;
        // Create the Apple event descriptor which points to the Finder target already.
        NSAppleEventDescriptor *finderEventDesc = [NSAppleEventDescriptor
appleEventWithEventClass:kAECoreSuite
eventID:kAECreateElement
argetDescriptor:finderPSNDesc
returnID:kAutoGenerateReturnID
transactionID:kAnyTransactionID];
        if (!finderEventDesc)
            break;
        // Create and add an event type descriptor: Alias 
        NSAppleEventDescriptor *osTypeDesc = [NSAppleEventDescriptor descriptorWithTypeCode:typeAlias];
        if (!osTypeDesc)
            break;
        [finderEventDesc setParamDescriptor:osTypeDesc forKeyword:keyAEObjectClass];
        // Now create the source Alias, which will be attached to the event.
        err = FSNewAliasFromPath(nil, [pstrSrcFile fileSystemRepresentation], 0, &hSrcAlias, 0);
        if (err != noErr)
            break;
        char handleState;
        handleState = HGetState((Handle)hSrcAlias);
        HLock((Handle)hSrcAlias);
        NSAppleEventDescriptor *srcAliasDesc = [NSAppleEventDescriptor
descriptorWithDescriptorType:typeAlias
bytes:*hSrcAlias
length:GetAliasSize(hSrcAlias)];
        if (!srcAliasDesc)
            break;
        [finderEventDesc setParamDescriptor:srcAliasDesc
forKeyword:keyASPrepositionTo];
        HSetState((Handle)hSrcAlias, handleState);
        // Next create the target Alias and attach it to the event.
        err = FSNewAliasFromPath(nil, [pstrDstPath fileSystemRepresentation], 0, &hDstAlias, 0);
        if (err != noErr)
            break;
        handleState = HGetState((Handle)hDstAlias);
        HLock((Handle)hDstAlias);
        NSAppleEventDescriptor *dstAliasDesc = [NSAppleEventDescriptor
descriptorWithDescriptorType:t ypeAlias
bytes:*hDstAlias
length:GetAliasSize(hDstAlias)];
        if (!dstAliasDesc)
            break;
        [finderEventDesc setParamDescriptor:dstAliasDesc
forKeyword:keyAEInsertHere];
        HSetState((Handle)hDstAlias, handleState);
        // Finally a property descriptor containing the target Alias name.
        NSAppleEventDescriptor *finderPropDesc = [NSAppleEventDescriptor recordDescriptor];
        if (!finderPropDesc)
            break;
        [finderPropDesc setDescriptor:[NSAppleEventDescriptor descriptorWithString:pstrName]
forKeyword:keyAEName];
        [finderEventDesc setParamDescriptor:finderPropDesc forKeyword:keyAEPropData];
        // Now send the event to the Finder.
        err = AESend([finderEventDesc aeDesc],
            NULL,
            kAENoReply,
            kAENormalPriority,
            kNoTimeOut,
            0,
            nil);
    } while(0);
    // Cleanup 
    if (hSrcAlias)
        DisposeHandle((Handle)hSrcAlias);
    if (hDstAlias)
        DisposeHandle((Handle)hDstAlias);
    return err == noErr ? true : false;
} */

bool QnMacShortcuts::createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments, int iconId)
{
    return false;
}

bool QnMacShortcuts::deleteShortcut(const QString &destinationPath, const QString &name) const {
    return true;
}

bool QnMacShortcuts::shortcutExists(const QString &destinationPath, const QString &name) const 
{
    return false;
}

bool QnMacShortcuts::supported() const {
    return false;
}
