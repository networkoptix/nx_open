/************************************************************************/
/*                                                                      */
/*                Centre for Speech Technology Research                 */
/*                     University of Edinburgh, UK                      */
/*                       Copyright (c) 1996,1997                        */
/*                        All Rights Reserved.                          */
/*                                                                      */
/*  Permission is hereby granted, free of charge, to use and distribute */
/*  this software and its documentation without restriction, including  */
/*  without limitation the rights to use, copy, modify, merge, publish, */
/*  distribute, sublicense, and/or sell copies of this work, and to     */
/*  permit persons to whom this work is furnished to do so, subject to  */
/*  the following conditions:                                           */
/*   1. The code must retain the above copyright notice, this list of   */
/*      conditions and the following disclaimer.                        */
/*   2. Any modifications must be clearly marked as such.               */
/*   3. Original authors' names are not deleted.                        */
/*   4. The authors' names are not used to endorse or promote products  */
/*      derived from this software without specific prior written       */
/*      permission.                                                     */
/*                                                                      */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK       */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE    */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      */
/*  THIS SOFTWARE.                                                      */
/*                                                                      */
/************************************************************************/
/*                 Author: Paul Taylor                                  */
/*                   Date: December 1997                                */
/************************************************************************/

#include "EST_Complex.h"
#include <iostream>
#include <cstdio>

int main()
{
    EST_Complex z1(4.0, 3.0);

    cout << z1 << "\n";
    cout << "real " << z1.real() << endl;
    cout << "imag " << z1.imag() << endl;

    cout << "mag " << z1.mag() << endl;
    cout << "ang in radians" << z1.ang() << endl;
    cout << "ang in degress " << z1.ang(1) << endl;

    EST_Complex z2(2.0, 2.0);
    EST_Complex z3;
    float x = 10.0;

    cout << "z1  = " << z1 << endl;
    cout << "z2  = " << z2 << endl;

    z3 = z1 + z2;
    cout << "z1 + z2 = " << z3 << endl;
    z3 = z1 + x;
    cout << "z3 + 10= " << z3 << endl;
    z3 = x + z1;
    cout << "10 + z3 = " << z3 << endl;

    z3 = z1 - z2;
    cout << "z1 - z2 = " << z3 << endl;
    z3 = z1 - x;
    cout << "z1 - 10= " << z3 << endl;
    z3 = x - z1;
    cout << "10 - z1 = " << z3 << endl;

    z3 = z1 * z2;
    cout << "z1 * z2 = " << z3 << endl;
    z3 = z1 * x;
    cout << "z1 * 10= " << z3 << endl;
    z3 = x * z1;
    cout << "10 * z1 = " << z3 << endl;

    z3 = z1 / z2;
    cout << "z1 / z2 = " << z3 << endl;
    z3 = z1 / x;
    cout << "z1 / 10= " << z3 << endl;
    z3 = x / z1;
    cout << "10 / z1 = " << z3 << endl;

    return 0;
}
