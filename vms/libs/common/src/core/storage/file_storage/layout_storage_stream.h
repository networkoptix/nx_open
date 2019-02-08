#pragma once

/**@file
 * Special stream extension functions that are specific for layout storage.
 * This class accompanies QIDevice stream. Ancestor for plain and crypto layout storage streams.
 */
class QnLayoutStreamSupport
{
public:
    virtual ~QnLayoutStreamSupport() = default;

    virtual void storeStateAndClose() = 0 ;
    virtual void restoreState() = 0;
};
