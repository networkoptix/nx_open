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
/*                       Date   :  30 Apr 2003                           */
/* --------------------------------------------------------------------- */
/*                                                                       */
/* Swig Python type maps for some speech tools types                     */
/*                                                                       */
/*************************************************************************/

%{
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif
%}

%typemap(in) EST_String & (EST_String temp) {
  char *str; Py_ssize_t len;
  PyString_AsStringAndSize($input, &str, &len);
  temp = EST_String( str, len, 0, len );
  $1 = &temp;
}

/* new - need typechecking for overloaded function dispatcher */
%typemap(typecheck) EST_String& = char *;

%typemap(in) EST_String {
  char *str; Py_ssize_t len;
  PyString_AsStringAndSize($input, &str, &len);
  $1 = EST_String( str, len, 0, len );
}
/* new - need typechecking for overloaded function dispatcher */
%typemap(typecheck) EST_String = char *;


%typemap(out) EST_String {
   int len = $1.length();
   $result = (len ? PyString_FromStringAndSize($1.str(),len) : Py_BuildValue((char*)""));

}
%typemap(out) EST_String&, EST_String* {
  int len = $1->length();
  $result = len ? PyString_FromStringAndSize($1->str(),len) : Py_BuildValue((char*)"");
}


// for now, just going to convert EST_StrList as a "one off", but
// maybe in future, the EST_TList class should be wrapped and
// then just the relevant typedefs made?
%typemap(in) EST_StrList & (EST_StrList temp) {
  if (PyList_Check($input)){
    int size = PyList_Size( $input );

    for(int i=0; i<size; ++i){
      PyObject *o = PyList_GetItem( $input, i );
      
      if( PyString_Check(o))
	temp.append( PyString_AsString(o) );
      else{
	PyErr_SetString( PyExc_TypeError, "needed list of strings" );
	return NULL;
      }
    }
  }
  else{
    PyErr_SetString( PyExc_TypeError, "not a list");
    return NULL;
  }
    
  $1 = &temp; 
}

// ITEM__LISTOUT maps are primarily for EST_Relation::items and EST_Item::leafs 
// EST_Item::daughters functions
// (this is a somewhat circuitous route, but it does leave a certain
// amount of flexibility in how the list is constructed in various 
// languages (according to the script language native API)
#ifndef HAVEITEMLISTOUT
#define HAVEITEMLISTOUT
%typemap(in,numinputs=0) EST_Item **ITEMLISTOUT (EST_Item *temp)
  "$1 = &temp;"

%typemap(argout) EST_Item **ITEMLISTOUT {
  PyObject *o1, *o2, *o3;

  o1 = PyList_New((*$1)->length());

  unsigned int i;
  EST_Item *it;
  for( it = (*$1), i=0; it!=0; it=it->next(), ++i )
    PyList_SetItem( o1, i, SWIG_NewPointerObj((void *) it, $descriptor(EST_Item*), 0) );

  if ((!$result) || ($result == Py_None)) {
    $result = o1;
  }
  else {
    if (!PyTuple_Check($result)) {
      o2 = $result;
      $result = PyTuple_New(1);
      PyTuple_SetItem($result, 0, o2);
    }

    o3 = PyTuple_New(1);
    PyTuple_SetItem( o3, 0, o1 );
    o2 = $result;
    $result = PySequence_Concat( o2, o3 );
    Py_DECREF(o2);
    Py_DECREF(o3);
  }
}
#endif

#ifndef HAVEITEMLEAFLISTOUT
#define HAVEITEMLEAFLISTOUT
%typemap(in,numinputs=0) EST_Item **ITEMLEAFLISTOUT = EST_Item **ITEMLISTOUT;
%typemap(argout) EST_Item **ITEMLEAFLISTOUT {
  PyObject *o1, *o2, *o3;

  EST_Item *first_item = first_leaf(*$1);
  EST_Item *last_item = last_leaf(*$1);
  EST_Item *it;
  unsigned int i, num_leafs;
  
  // faster to count leafs first and allocate PyList once (?)
  for( it=first_item, num_leafs=0; (it!=0) && (it!=last_item); it=next_leaf(it) )
    ++num_leafs;
  
  if( it != 0 )
    ++num_leafs; // for last_item

  switch (num_leafs) {
  case 0:
    o1 = Py_None;  //not sure this will ever happen
    break;
//   case 1:
//     o1 = SWIG_NewPointerObj((void *) it, SWIGTYPE_p_EST_Item, 0);
//     break;
  default:
    o1 = PyList_New(num_leafs);
    for( i=0, it=first_item; i<num_leafs; ++i, it=next_leaf(it) )
      PyList_SetItem( o1, i, SWIG_NewPointerObj((void *) it, $descriptor(EST_Item*), 0) );
  }

  if ((!$result) || ($result == Py_None)) {
    $result = o1;
  }
  else {
    if (!PyTuple_Check($result)) {
      o2 = $result;
      $result = PyTuple_New(1);
      PyTuple_SetItem($result, 0, o2);
    }

    o3 = PyTuple_New(1);
    PyTuple_SetItem( o3, 0, o1 );
    o2 = $result;
    $result = PySequence_Concat( o2, o3 );
    Py_DECREF(o2);
    Py_DECREF(o3);
  }
}
#endif

#ifndef HAVEITEMDAUGHTERLISTOUT
#define HAVEITEMDAUGHTERLISTOUT
%typemap(in,numinputs=0) EST_Item **ITEMDAUGHTERLISTOUT = EST_Item **ITEMLISTOUT;
%typemap(argout) EST_Item **ITEMDAUGHTERLISTOUT {
  PyObject *o1, *o2, *o3;

  EST_Item *first_item = daughter1(*$1);
  EST_Item *last_item = daughtern(*$1); //could eliminate this call(does same traverse as below)
  EST_Item *it;
  unsigned int i, num_daughters;
  
  // faster to count daughters first and allocate PyList once (?)
  for( it=first_item, num_daughters=0; (it!=0) && (it!=last_item); it=next_sibling(it) )
    ++num_daughters;
  
  if( it != 0 )
    ++num_daughters; // for last_item

  switch (num_daughters) {
  case 0:
    o1 = Py_None;
    break;
//   case 1:
//     o1 = SWIG_NewPointerObj((void *) it, SWIGTYPE_p_EST_Item, 0);
//     break;
  default:
    o1 = PyList_New(num_daughters);
    for( i=0, it=first_item; i<num_daughters; ++i, it=next_sibling(it) )
      PyList_SetItem( o1, i, SWIG_NewPointerObj((void *) it, $descriptor(EST_Item*), 0) );
  }

  if ((!$result) || ($result == Py_None)) {
    $result = o1;
  }
  else {
    if (!PyTuple_Check($result)) {
      o2 = $result;
      $result = PyTuple_New(1);
      PyTuple_SetItem($result, 0, o2);
    }

    o3 = PyTuple_New(1);
    PyTuple_SetItem( o3, 0, o1 );
    o2 = $result;
    $result = PySequence_Concat( o2, o3 );
    Py_DECREF(o2);
    Py_DECREF(o3);
  }
}
#endif



