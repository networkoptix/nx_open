/*************************************************************************/
/*                                                                       */
/*                  Language Technologies Institute                      */
/*                     Carnegie Mellon University                        */
/*                         Copyright (c) 2013                            */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author:  Alok Parlikar (aup@cs.cmu.edu)                   */
/*               Date:  April 2013                                       */
/*************************************************************************/
/*
  Support to call Python Functions from SIOD
*/

#ifdef EST_SIOD_ENABLE_PYTHON
#include "slib_python.h"
#include "siod.h"

#include "Python.h"

// The following are the types for Python objects in LISP, they are
// set when the objects are registered.  These are not required
// outside this file, hence static.
static int tc_pyobject = -1;

// Check if a LISP object stores reference to PyObject
int pyobject_p(LISP x) {
  if (TYPEP(x, tc_pyobject))
    return TRUE;
  return FALSE;
}

LISP pyobjectp(LISP x) {
  if (pyobject_p(x))
    return truth;
  return NIL;
}

// This always returns a new reference to PyObject
// If it already stores a reference to a PyObject
// This increments its count.
static PyObject *get_c_pyobject(LISP x) {
  if (TYPEP(x, tc_pyobject)) {
    PyObject *p = reinterpret_cast<PyObject *>(USERVAL(x));
    Py_XINCREF(p);
    return p;
  }

  if (NULLP(x))
    Py_RETURN_NONE;

  if (numberp(x))
    return PyFloat_FromDouble(get_c_double(x));

  if (TYPEP(x, tc_string))
    return PyUnicode_FromString(get_c_string(x));

  if (consp(x)) {
    int num_items = siod_llength(x);
    PyObject *pList = PyList_New(num_items);
    LISP ptr;
    int i;
    for (ptr = x, i = 0;
        NNULLP(ptr);
        ptr = cdr(ptr), i++) {
      PyList_SetItem(pList, i, get_c_pyobject(car(ptr)));
    }
    return pList;
  }

  err("wrong type of argument to get_c_pyobject", x);
  return NULL;  // err doesn't return but compilers don't know that
}

static LISP siod_make_pyobject(PyObject *pyobj) {
  if (pyobj == NULL || pyobj == Py_None)
    return NIL;

  if (PyLong_Check(pyobj) || PyFloat_Check(pyobj))
    return flocons(PyFloat_AsDouble(pyobj));

  if (PyBool_Check(pyobj))
    return PyObject_IsTrue(pyobj)? truth : NIL;

  if (PyUnicode_Check(pyobj)) {
    PyObject *pBytes;
    LISP ret;
    pBytes = PyUnicode_AsUTF8String(pyobj);
    if (pBytes == NULL)
      return NIL;

    ret = strcons(PyBytes_Size(pBytes),
                  PyBytes_AsString(pBytes));
    Py_DECREF(pBytes);
    return ret;
  }

  if (PyTuple_Check(pyobj) || PyList_Check(pyobj)) {
    LISP ret = NIL;
    int size = PySequence_Size(pyobj);
    if (size <= 0)
      return NIL;
    for (int i = size - 1; i >= 0; i--)
      ret = cons(siod_make_pyobject(PySequence_GetItem(pyobj, i)),
                 ret);
    return ret;
  }

  // Bytes, Dict, or Other Objects are stored as Python Objects.
  Py_XINCREF(pyobj);
  return siod_make_typed_cell(tc_pyobject, pyobj);
}

static void pyobject_free(LISP x) {
  // Decrement refcount if x stores a PyObject;
  if (TYPEP(x, tc_pyobject)) {
    PyObject *p = reinterpret_cast<PyObject *>(USERVAL(x));
    Py_XDECREF(p);
  }
}

static void pyobject_prin1(LISP v, FILE *fp) {
  if (TYPEP(v, tc_pyobject)) {
    PyObject *p = reinterpret_cast<PyObject *>(USERVAL(v));
    PyObject_Print(p, fp, Py_PRINT_RAW);
  }
}

static void pyobject_print_string(LISP v, char *tkbuffer) {
  if (TYPEP(v, tc_pyobject)) {
     PyObject *p = reinterpret_cast<PyObject *>(USERVAL(v));
     PyObject *pRepr = PyObject_Str(p);
     if (pRepr == NULL) {
       snprintf(tkbuffer, 1024, "#<UnknownPythonObject %p>", p);  // NOLINT
       return;
     }

     LISP repr = siod_make_pyobject(pRepr);
     snprintf(tkbuffer, 1024, "PyObject %s", get_c_string(repr));  // NOLINT
     Py_DECREF(pRepr);
     return;
  }
  snprintf(tkbuffer, 1024, "#<UnknownObject>"); // NOLINT
}

static LISP python_syspath_append(LISP path) {
  if (!TYPEP(path, tc_string)) {
    err("Invalid Path", path);
    return NIL;
  }

  PyObject* sysPath = PySys_GetObject("path");
  int ret = PyList_Append(sysPath, PyUnicode_FromString(get_c_string(path)));
  if (ret == 0) {
    // Success
    return truth;
  }
  return NIL;
}

static LISP python_import(LISP modulename) {
  PyObject *pName, *pModule;
  LISP ret;

  if (!TYPEP(modulename, tc_string)) {
    err("Invalid module name (expecting string)", modulename);
    return NIL;
  }

  pName = PyUnicode_FromString(get_c_string(modulename));
  pModule = PyImport_Import(pName);
  Py_XDECREF(pName);

  if (pModule == NULL) {
    if (PyErr_Occurred()) {
      PyErr_Print();
      PyErr_Clear();
    }
    err("Failed to load module", modulename);
    return NIL;
  }
  ret = siod_make_pyobject(pModule);
  Py_DECREF(pModule);
  return ret;
}

static LISP python_attr_get(LISP lpobj, LISP attrname) {
  if (!TYPEP(lpobj, tc_pyobject)) {
    err("Invalid Object for python_attr_get", lpobj);
    return NIL;
  }

  PyObject *p = reinterpret_cast<PyObject *>(USERVAL(lpobj));

  if (!TYPEP(attrname, tc_string)) {
    err("Invalid Attribute Name (expecting string)", attrname);
    return NIL;
  }

  PyObject *pAttr = PyObject_GetAttrString(p, get_c_string(attrname));
  if (pAttr == NULL) {
    if (PyErr_Occurred()) {
      PyErr_Print();
      PyErr_Clear();
    }
  }
  LISP ret = siod_make_pyobject(pAttr);
  Py_XDECREF(pAttr);
  return ret;
}

static LISP python_attr_set(LISP lpobj, LISP attrname, LISP value) {
  if (!TYPEP(lpobj, tc_pyobject)) {
    err("Invalid Object for python_attr_set", lpobj);
    return NIL;
  }

  PyObject *p = reinterpret_cast<PyObject *>(USERVAL(lpobj));

  if (!TYPEP(attrname, tc_string)) {
    err("Invalid Attribute Name (expecting string)", attrname);
    return NIL;
  }

  PyObject *pValue = get_c_pyobject(value);
  if (pValue == NULL) {
    if (PyErr_Occurred()) {
      PyErr_Print();
      PyErr_Clear();
    }
    err("Invalid Value for python_attr_set", value);
    return NIL;
  }

  int result = PyObject_SetAttrString(p, get_c_string(attrname), pValue);
  Py_DECREF(pValue);

  if (result == -1) {
    err("Failed to set value", value);
    return NIL;
  }
  return truth;
}

static LISP python_call_object(LISP fpobj, LISP args) {
  if (!TYPEP(fpobj, tc_pyobject)) {
    err("Invalid Object for python_callfunction", fpobj);
    return NIL;
  }

  PyObject *p = reinterpret_cast<PyObject *>(USERVAL(fpobj));

  if (p == NULL || !PyCallable_Check(p)) {
    err("Not a callable object", fpobj);
    return NIL;
  }

  PyObject *pArgs;
  if (args == NIL) {
    pArgs = NULL;
  } else {
    if (!consp(args)) {
      err("Invalid argument (expecting list)", args);
      return NIL;
    }

    pArgs = get_c_pyobject(args);
    if (pArgs == NULL) {
      err("Could not convert arguments", args);
      return NIL;
    }
  }

  PyObject *pArgsTuple = NULL;
  if (pArgs != NULL) {
    pArgsTuple = PyList_AsTuple(pArgs);
    Py_XDECREF(pArgs);
  }

  PyObject *pValue = PyObject_CallObject(p, pArgsTuple);
  Py_XDECREF(pArgsTuple);

  if (pValue == NULL) {
    if (PyErr_Occurred()) {
      PyErr_Print();
      PyErr_Clear();
    }
    err("Could not call object", fpobj);
    return NIL;
  }
  LISP ret = siod_make_pyobject(pValue);
  Py_DECREF(pValue);
  return ret;
}

static LISP python_call_method(LISP lpobj, LISP methodname, LISP args) {
  LISP callable = python_attr_get(lpobj, methodname);
  return python_call_object(callable, args);
}

void init_subrs_python(void) {
  Py_Initialize();

  long kind;  // NOLINT

  tc_pyobject = siod_register_user_type("PyObject");
  set_gc_hooks(tc_pyobject, 0, NULL, NULL, NULL, pyobject_free, NULL, &kind);
  set_print_hooks(tc_pyobject, pyobject_prin1, pyobject_print_string);

  // Add CWD to PythonPath
  PyObject* sysPath = PySys_GetObject("path");
  PyList_Append(sysPath, PyUnicode_FromString("."));

  init_subr_1("pyobjectp", pyobjectp,
              "(pyobjectp obj)\n"
              "Checks if obj is a Python Object");


  init_subr_1("python_syspath_append", python_syspath_append,
              "(python_addpath path)\n"
              "Appends path (string) to sys.path");

  init_subr_1("python_import", python_import,
              "(python_import modulename)\n"
              "Imports specified module and returns it");

  init_subr_2("python_attr_get", python_attr_get,
              "(python_attr_get object attrname)\n"
              "Returns the specified attribute of the given PyObject");

  init_subr_3("python_attr_set", python_attr_set,
              "(python_attr_set object attrname value)\n"
              "Set value of the given attribute of the given PyObject");

  init_subr_3("python_call_method", python_call_method,
              "(python_call_method object methodname args)\n"
              "Calls  object.methodname(args)\n"
              "object is a PyObject, methodname is string. args is a list.");

  init_subr_2("python_call_object", python_call_object,
              "(python_call_object object args)\n"
              "Calls  object(args)\n"
              "object is a callable PyObject, args is a list");
}

void python_tidy_up(void) {
  Py_Finalize();
}
#else   // No python support

/* So there is a symbol in here even if there is no python support */
static int est_no_python_support = 1;

#endif  // EST_SIOD_ENABLE_PYTHON
