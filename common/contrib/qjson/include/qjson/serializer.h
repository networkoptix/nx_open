/* This file is part of qjson
  *
  * Copyright (C) 2009 Till Adam <adam@kde.org>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License version 2.1, as published by the Free Software Foundation.
  * 
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
  */

#ifndef QJSON_SERIALIZER_H
#define QJSON_SERIALIZER_H

#include "qjson_export.h"

class QIODevice;
class QString;
class QVariant;

namespace QJson {
  /**
  * @brief How the indentation should work.
  *
  * none (default) : { "foo" : 0, "foo1" : 1, "foo2" : [ { "foo3" : 3, "foo4" : 4 } ] }
  *
  * compact : {"foo":0,"foo1":1,"foo2":[{"foo3":3,"foo4":4}]}
  *
  * minimum : { "foo" : 0, "foo1" : 1, "foo2" : [
  *             { "foo3" : 3, "foo4" : 4 }
  *           ] }
  *
  * medium : {
  *           "foo" : 0, "foo1" : 1, "foo2" : [
  *            {
  *             "foo3" : 3, "foo4" : 4
  *            }
  *           ]
  *          }
  * full : {
  *         "foo" : 0,
  *         "foo1" : 1,
  *         "foo2" : [
  *          {
  *           "foo3" : 3,
  *           "foo4" : 4
  *          }
  *         ]
  *        }
  */
  enum IndentMode {
    IndentNone,
    IndentCompact,
    IndentMinimum,
    IndentMedium,
    IndentFull
  };
  /**
  * @brief Main class used to convert QVariant objects to JSON data.
  *
  * QVariant objects are converted to a string containing the JSON data.
  * If QVariant object is empty or not valid a <em>null</em> json object is returned.
  */
  class QJSON_EXPORT Serializer {
  public:
    Serializer();
    ~Serializer();

     /**
      * This method generates a textual JSON representation and outputs it to the
      * passed in I/O Device.
      * @param variant The JSON document in its in-memory representation as generated by the
      * parser.
      * @param out Input output device
      * @param ok if a conversion error occurs, *ok is set to false; otherwise *ok is set to true
      */
    void serialize( const QVariant& variant, QIODevice* out, bool* ok = 0);

    /**
      * This is a method provided for convenience. It turns the passed in in-memory
      * representation of the JSON document into a textual one, which is returned.
      * If the returned string is empty, the document was empty. If it was null, there
      * was a parsing error.
      *
      * @param variant The JSON document in its in-memory representation as generated by the
      * parser.
      */

    QByteArray serialize( const QVariant& variant);

    /**
     * Allow or disallow writing of NaN and/or Infinity (as an extension to QJson)
     */
    void allowSpecialNumbers(bool allow);

    /**
     * Is Nan and/or Infinity allowed?
     */
    bool specialNumbersAllowed() const;

    /**
     * set output indentation mode as defined in QJson::IndentMode
     */
    void setIndentMode(IndentMode mode = QJson::IndentNone);


    /**
    * set double precision used while converting Double
    * \sa QByteArray::number
    */
    void setDoublePrecision(int precision);

    /**
     * Returns one of the indentation modes defined in QJson::IndentMode
     */
    IndentMode indentMode() const;

  private:
    Q_DISABLE_COPY(Serializer)
    class SerializerPrivate;
    SerializerPrivate* const d;
  };
}

#endif // QJSON_SERIALIZER_H
