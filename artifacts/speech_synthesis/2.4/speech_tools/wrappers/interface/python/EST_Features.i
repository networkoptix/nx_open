/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                 (University of Edinburgh, UK) and                     */
/*                           Korin Richmond                              */
/*                         Copyright (c) 2003                            */
/*                         All Rights Reserved.                          */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*                                                                       */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                       Author :  Korin Richmond                        */
/*                       Date   :  15 May 2003                           */
/* --------------------------------------------------------------------- */
/*                                                                       */
/* Swig Python type maps for EST_Features class                          */
/* (converts to/from Python Dictionary object)                           */
/*                                                                       */
/*************************************************************************/

%{
  static PyObject* features_to_PyDict( EST_Features *features ){
    
    EST_Features::RwEntries p;
    PyObject *result = PyDict_New();
    
    for( p.begin(*features); p != 0; ++p ){
      const EST_Val &v = p->v;
      
      PyObject *py_val;
      if(v.type() == val_unset)
	py_val = Py_None;
 
      else if( v.type() == val_int )
	py_val = PyInt_FromLong( v.Int() );

      else if( v.type() == val_float )
	py_val = PyFloat_FromDouble( v.Float() );

      else if( v.type() == val_type_feats )
	py_val = features_to_PyDict( feats(v) );

//       else if( v.type() == val_type_featfunc ){
// 	if( evaluate_ff ){
// 	  EST_Val tempval = ((featfunc)(*v))(##SHOULD BE AN ITEM##);
// 	  if( tempval.type() == val_int )
// 	    py_val = PyInt_FromLong( tempval.Int() );
	  
// 	  else if( tempval.type() == val_float )
// 	    py_val = PyFloat_FromDouble( tempval.Float() );
	  
// 	  else
// 	    py_val = PyString_FromString( tempval.string() );
// 	}
// 	else
// 	  py_val = PyString_FromString( v.string() );
//       }

      else{
	EST_String s( v.string() );
	if ( s.length() == 0 )
	  py_val = Py_BuildValue((char*)"");
	else
	  py_val = PyString_FromStringAndSize( s.str(), s.length() );
      }

      PyDict_SetItemString( result, (char*)(p->k), py_val );
    }
    return result;
  }
%}

%{
  static PyObject* item_features_to_PyDict( EST_Item *item, bool evaluate_ff ){
    
    EST_Features::RwEntries p;
    PyObject *result = PyDict_New();
    
    for( p.begin(item->features()); p != 0; ++p ){
      const EST_Val &v = p->v;
      
      PyObject *py_val;
      if(v.type() == val_unset)
	py_val = Py_None;
 
      else if( v.type() == val_int )
	py_val = PyInt_FromLong( v.Int() );

      else if( v.type() == val_float )
	py_val = PyFloat_FromDouble( v.Float() );

      else if( v.type() == val_type_feats )
	py_val = features_to_PyDict( feats(v) );

      else if( (v.type() == val_type_featfunc) && evaluate_ff ){
	EST_Val tempval = ((featfunc)(v))(item);
	if( tempval.type() == val_int )
	  py_val = PyInt_FromLong( tempval.Int() );
	
	else if( tempval.type() == val_float )
	  py_val = PyFloat_FromDouble( tempval.Float() );
	
	else{
	  EST_String s( tempval.string() );
	  if ( s.length() == 0 )
	    py_val = Py_BuildValue((char*)"");
	  else
	    py_val = PyString_FromStringAndSize( s.str(), s.length() );
	}
      }
      else{
	EST_String s( v.string() );
	if ( s.length() == 0 )
	  py_val = Py_BuildValue((char*)"");
	else
	  py_val = PyString_FromStringAndSize( s.str(), s.length() );
      }
      
      PyDict_SetItemString( result, (char*)(p->k), py_val );
    }
    return result;
  }
%}

%typemap(out) EST_Features &
  "$result = features_to_PyDict( $1 );";

// #ifndef HAVEITEMFEATURESFUNCTION
// #define HAVEITEMFEATURESFUNCTION
// typedef struct {
//   EST_Item *it;
//   bool evaluate_ff;
// } FUNCTIONARGS_ITEMFEATURES;

// %typemap(out) FUNCTION {
//   $result = item_features_to_PyDict( arg1, arg2 );
// }
// #endif

