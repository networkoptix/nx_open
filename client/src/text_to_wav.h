
#ifndef TEXT_TO_WAV_H
#define TEXT_TO_WAV_H

#include <QIODevice>
#include <QString>


//!Synthesizes wav based on \a text. Uses festival engine
/*!
    \param text Only latin1 string is supported
    \note This method is synchronous
    \note This function is reenterable
*/
bool textToWav( const QString& text, QIODevice* const dest );

#endif  //TEXT_TO_WAV_H
