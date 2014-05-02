////////////////////////////////////////////////////////////
// 1 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTDECODERMANAGER_H
#define ABSTRACTDECODERMANAGER_H

#include "stree/resourcecontainer.h"


class QnAbstractVideoDecoder;

/*!
    Decoder calls methods of this class in case of some events
*/
class AbstractDecoderEventReceiver
{
public:
    // TODO: #Elric #enum
    enum DecoderBehaviour
    {
        dbContinueHardWork,
        dbStop
    };

    //!Stream params changed
    /*!
        \return Thing to do for decoder
        \note Decoder has not impled new stream params yet
    */
    virtual DecoderBehaviour streamParamsChanged(
        QnAbstractVideoDecoder* decoder,
        const stree::AbstractResourceReader& newStreamParams ) = 0;
};

#endif  //ABSTRACTDECODERMANAGER_H
