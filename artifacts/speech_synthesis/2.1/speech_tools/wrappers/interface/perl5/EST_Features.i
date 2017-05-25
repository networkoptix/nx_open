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
/*                       Date   :  14 June 2003                          */
/* --------------------------------------------------------------------- */
/*                                                                       */
/* Swig Perl type maps for EST_Features class                            */
/* (converts to/from Perl Hash object for the moment)                    */
/*                                                                       */
/*************************************************************************/

%{
  static HV* features_to_PerlHash( EST_Features *features ){
    
    EST_Features::RwEntries p;
    HV *result = newHV();
    SV *value;
    
    for( p.begin(*features); p != 0; ++p ){
      const EST_Val &v = p->v;
      
      if(v.type() == val_unset)
	value = &PL_sv_undef;
 
      else if( v.type() == val_int )
	value = newSViv( v.Int() );

      else if( v.type() == val_float )
	value = newSVnv( v.Float() );

      else if( v.type() == val_type_feats )
	value = newRV_noinc((SV*) features_to_PerlHash(feats(v)) );

//       else if( v.type() == val_type_featfunc ){
// 	if( evaluate_ff ){
// 	  EST_Val tempval = ((featfunc)(*v))(##SHOULD BE AN ITEM##);
// 	  if( tempval.type() == val_int )
//	    value = newSViv( tempval.Int() );
//	  
// 	  else if( tempval.type() == val_float )
// 	    value = newSVnv( tempval.Float() );
//	  
// 	  else{
//          EST_String s( tempval.string() ); 
//	    value = newSVpvn( s.str(), s.length() );
//        }
// 	}
//      else{
//	  EST_String s( v.string() ); 
//	  value = newSVpvn( s.str(), s.length() );
//    }

      else{
	EST_String s( v.string() ); 
	value = newSVpvn( s.str(), s.length() );
      }
    
      char *key = (char*)(p->k);

      // should we check for duplicates before doing the store?
      hv_store(result, key, strlen(key), value, 0);
    }
    return result;
  }
%}


// for now at least, we'll just create a reference to a Perl Hash
// and populate it with the data from the EST_Features object
%typemap(out) EST_Features &
  "ST(argvi) = sv_newmortal();
   sv_setsv( (SV*)ST(argvi++), (SV*)newRV_noinc((SV*)features_to_PerlHash($1)) );";
