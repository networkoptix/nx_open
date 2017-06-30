/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1997                            */
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
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                     Author :  Alan W Black                            */
/*                     Date   :  November 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* A class for representing Weighted Finite State Transducers            */
/*                                                                       */
/* This is based on various papers by Mehryar Mohri but not forgetting   */
/* the Kay and Kaplan stuff as well as my own Koskenniemi implementation */
/* and finite state machine manipulations in my earlier lives            */
/*                                                                       */
/*=======================================================================*/

#include <iostream>
#include "EST_Pathname.h"
#include "EST_cutils.h"
#include "EST_Token.h"
#include "EST_FileType.h"
#include "EST_WFST.h"

#include "EST_TVector.h"

Declare_TList_T(EST_WFST_Transition *, EST_WFST_TransitionP)
Declare_TVector_Base_T(EST_WFST_State *, NULL, NULL, EST_WFST_StateP)


#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TList.cc"

Instantiate_TList_T(EST_WFST_Transition *, EST_WFST_TransitionP)

#include "../base_class/EST_TVector.cc"

Instantiate_TVector_T(EST_WFST_State *, EST_WFST_StateP)

#endif

// Used for marking states duration traversing functions
int EST_WFST::traverse_tag = 0;

EST_WFST_State::EST_WFST_State(int name)
{
    p_name = name;
    p_type = wfst_error;
    p_tag = 0;
}

EST_WFST_State::EST_WFST_State(const EST_WFST_State &state)
{
    EST_Litem *p;

    p_name = state.p_name;
    p_type = state.p_type;
    p_tag = state.p_tag;
    for (p=state.transitions.head(); p != 0; p=p->next())
	transitions.append(new EST_WFST_Transition(*state.transitions(p)));
}

EST_WFST_State::~EST_WFST_State()
{
    EST_Litem *p;

    for (p=transitions.head(); p != 0; p=p->next())
	delete transitions(p);

}

EST_WFST_Transition *EST_WFST_State::add_transition(float w,
						    int end, 
						    int in,
						    int out)
{
    // Add new transition
    EST_WFST_Transition *s = new EST_WFST_Transition(w,end,in,out);
    transitions.append(s);
    return s;
}

EST_WFST::~EST_WFST()
{
    clear();
}

void EST_WFST::clear()
{

    // delete up to p_num_states, rather than p_states.length() as
    // only up to there is necessarily filled
    for (int i=0; i < p_num_states; ++i)
	delete p_states[i];
    p_num_states = 0;
    p_cumulate = 0;
}

EST_WFST::EST_WFST()
{
    p_num_states = 0;
    init(0);
}

void EST_WFST::copy(const EST_WFST &wfst)
{
    clear();
    p_in_symbols = wfst.p_in_symbols;
    p_out_symbols = wfst.p_out_symbols;
    p_start_state = wfst.p_start_state;
    current_tag = wfst.current_tag;
    p_num_states = wfst.p_num_states;
    p_states.resize(p_num_states);
    for (int i=0; i < p_num_states; ++i)
	p_states[i] = new EST_WFST_State(*wfst.state(i));
}

void EST_WFST::init(int init_num_states)
{
    int i;

    clear();

    p_states.resize(init_num_states);
    for (i=0; i < p_states.length(); i++)
	p_states[i] = 0;
    p_num_states = init_num_states;

}

void EST_WFST::init(LISP in_alphabet,LISP out_alphabet)
{
    EST_StrList in,out;
    LISP iin,oout;

    in.append("__epsilon__");
    in.append("=");
    for (iin=in_alphabet; iin != NIL; iin=cdr(iin))
	if ((!streq(get_c_string(car(iin)),"__epsilon__")) &&
	    (!streq(get_c_string(car(iin)),"=")))
	    in.append(get_c_string(car(iin)));

    out.append("__epsilon__");
    out.append("=");
    for (oout=out_alphabet; oout != NIL; oout=cdr(oout))
	if ((!streq(get_c_string(car(oout)),"__epsilon__")) &&
	    (!streq(get_c_string(car(oout)),"=")))
	    out.append(get_c_string(car(oout)));

    p_in_symbols.init(in);
    p_out_symbols.init(out);

}

int EST_WFST::transduce(int state,const EST_String &in,EST_String &out) const
{
    int nstate;
    int in_i = p_in_symbols.name(in);
    int out_i=0;

    if (in_i == -1)
    {
	cerr << "WFST transduce: \"" << in << "\" not in alphabet" << endl;
	return WFST_ERROR_STATE;
    }
    
    nstate = transduce(state,in_i,out_i);

    out = p_out_symbols.name(out_i);

    return nstate;
}

void EST_WFST::transduce(int state,int in,wfst_translist &out) const
{
    EST_WFST_State *s = p_states(state);
    EST_Litem *i;

    for (i=s->transitions.head(); i != 0; i=i->next())
    {
	if (in == s->transitions(i)->in_symbol())
	{
	    if (cumulate())
		s->transitions(i)->set_weight(1+s->transitions(i)->weight());
	    out.append(s->transitions(i));
	}
    }

    // could return if any transitions were found
}

int EST_WFST::transduce(int state,int in,int &out) const
{
    EST_WFST_State *s = p_states(state);
    EST_Litem *i;

    for (i=s->transitions.head(); i != 0; i=i->next())
    {
	if (in == s->transitions(i)->in_symbol())
	{
	    out = s->transitions(i)->out_symbol();
	    return s->transitions(i)->state();
	}
    }

    return WFST_ERROR_STATE; // no match
}

int EST_WFST::transition(int state,const EST_String &inout) const
{
    if (inout.contains("/"))
	return transition(state,inout.before("/"),inout.after("/"));
    else
	return transition(state,inout,inout);
}

int EST_WFST::transition(int state,const EST_String &in,
			 const EST_String &out) const
{
    int in_i = p_in_symbols.name(in);
    int out_i = p_out_symbols.name(out);

    if ((in_i == -1) || (out_i == -1))
    {
	cerr << "WFST: one of " << in << "/" << out << " not in alphabet"
	    << endl;
	return WFST_ERROR_STATE;
    }

    return transition(state,in_i,out_i);
}

int EST_WFST::transition(int state,int in, int out) const
{
    // Finds first transition (hopefully deterministic)
    float prob;
    return transition(state,in,out,prob);
}

EST_WFST_Transition *EST_WFST::find_transition(int state,int in, int out) const
{
    // Finds first transition (hopefully deterministic)
    EST_WFST_State *s = p_states(state);
    EST_Litem *i;

    for (i=s->transitions.head(); i != 0; i=i->next())
    {
	if ((in == s->transitions(i)->in_symbol()) &&
	    (out == s->transitions(i)->out_symbol()))
	{
	    if (cumulate())
		s->transitions(i)->set_weight(1+s->transitions(i)->weight());
	    return s->transitions(i);
	}
    }

    return 0; // no match
}

int EST_WFST::transition(int state,int in, int out,float &prob) const
{
    // Finds first transition (hopefully deterministic)
    EST_WFST_Transition *trans = find_transition(state,in,out);

    if (trans == 0)
    {
	prob = 0;
	return WFST_ERROR_STATE;
    }
    else
    {
	prob = trans->weight();
	return trans->state();
    }
}

EST_write_status EST_WFST::save_binary(FILE *fd)
{
    int i;
    EST_Litem *j;
    int num_transitions, type, in, out, next_state;
    float weight;
    
    for (i=0; i<p_num_states; i++)
    {
	num_transitions = p_states[i]->num_transitions();
	fwrite(&num_transitions,4,1,fd);
	if (p_states[i]->type() == wfst_final)
	    type = WFST_FINAL;
	else if (p_states[i]->type() == wfst_nonfinal)
	    type = WFST_NONFINAL;
	else if (p_states[i]->type() == wfst_licence)
	    type = WFST_LICENCE;
	else
	    type = WFST_ERROR;
	fwrite(&type,4,1,fd);
	for (j=p_states[i]->transitions.head(); j != 0; j=j->next())
	{
	    in = p_states[i]->transitions(j)->in_symbol();
	    out = p_states[i]->transitions(j)->out_symbol();
	    next_state = p_states[i]->transitions(j)->state();
	    weight = p_states[i]->transitions(j)->weight();

	    if (in == out)
	    {
		in *= -1;
		fwrite(&in,4,1,fd);
	    }
	    else
	    {
		fwrite(&in,4,1,fd);
		fwrite(&out,4,1,fd);
	    }
	    fwrite(&next_state,4,1,fd);
	    fwrite(&weight,4,1,fd);
	}
    }

    return write_ok;
}

EST_write_status EST_WFST::save(const EST_String &filename,
				const EST_String type)
{
    FILE *ofd;
    int i;
    static EST_Regex needquotes(".*[()'\";., \t\n\r].*");
    EST_Litem *j;

    if (filename == "-")
	ofd = stdout;
    else if ((ofd = fopen(filename,"wb")) == NULL)
    {
	cerr << "WFST: cannot write to file \"" << filename << "\"" << endl;
	return misc_write_error;
    }
    
    fprintf(ofd,"EST_File fst\n");
    fprintf(ofd,"DataType %s\n",(const char *)type);
    fprintf(ofd,"in %s\n",
      (const char *)quote_string(EST_String("(")+
				 p_in_symbols.print_to_string(TRUE)+")",
				 "\"","\\",1));
    fprintf(ofd,"out %s\n",
      (const char *)quote_string(EST_String("(")+
				 p_out_symbols.print_to_string(TRUE)+")",
				 "\"","\\",1));
    fprintf(ofd,"NumStates %d\n",p_num_states);
    fprintf(ofd, "ByteOrder %s\n", ((EST_NATIVE_BO == bo_big) ? "10" : "01"));
    fprintf(ofd,"EST_Header_End\n");

    if (type == "binary")
	save_binary(ofd);
    else
    {
	for (i=0; i < p_num_states; i++)
	{
	    EST_WFST_State *s=p_states[i];
	    fprintf(ofd,"((%d ",s->name());
	    switch(s->type())
	    {
	    case wfst_final: 
		fprintf(ofd,"final ");
		break;
	    case wfst_nonfinal: 
		fprintf(ofd,"nonfinal ");
		break;
	    case wfst_licence: 
		fprintf(ofd,"licence ");
		break;
	    default: 
		fprintf(ofd,"error ");
	    }
	    fprintf(ofd,"%d)\n",s->num_transitions());
	    for (j=s->transitions.head(); j != 0; j=j->next())
	    {
		EST_String in = p_in_symbols.name(s->transitions(j)->in_symbol());
		EST_String out=p_out_symbols.name(s->transitions(j)->out_symbol());
		if (in.matches(needquotes))
		    fprintf(ofd,"  (%s ",(const char *)quote_string(in,"\"","\\",1));
		else
		    fprintf(ofd,"  (%s ",(const char *)in);
		if (out.matches(needquotes))
		    fprintf(ofd," %s ",(const char *)quote_string(out,"\"","\\",1));
		else
		    fprintf(ofd," %s ",(const char *)out);
		fprintf(ofd,"%d %g)\n",
			s->transitions(j)->state(),
			s->transitions(j)->weight());
	    }
	    fprintf(ofd,")\n");
	}
    }
    if (ofd != stdout)
	fclose(ofd);

    return write_ok;
}

static float get_float(FILE *fd,int swap)
{
    float f;
    fread(&f,4,1,fd);
    if (swap) swapfloat(&f);
    return f;
}

static int get_int(FILE *fd,int swap)
{
    int i;
    fread(&i,4,1,fd);
    if (swap) 
	return SWAPINT(i);
    else
	return i;
}

EST_read_status EST_WFST::load_binary(FILE *fd,
				      EST_Option &hinfo,
				      int num_states,
				      int swap)
{
    EST_read_status r;
    int i,j, s;
    int num_trans, state_type;
    int in_sym, out_sym, next_state;
    float trans_cost;

    r = format_ok;

    for (i=0; i < num_states; i++)
    {
	num_trans = get_int(fd,swap);
	state_type = get_int(fd,swap);
	
	if (state_type == WFST_FINAL)
	    s = add_state(wfst_final);
	else if (state_type == WFST_NONFINAL)
	    s = add_state(wfst_nonfinal);
	else if (state_type == WFST_LICENCE)
	    s = add_state(wfst_licence);
	else if (state_type == WFST_ERROR)
	    s = add_state(wfst_error);
	else
	{
	    cerr << "WFST load: unknown state type \"" << 
		state_type << "\"" << endl;
	    r = read_format_error;
	    break;
	}

	if (s != i)
	{
	    cerr << "WFST load: internal error: unexpected state misalignment"
		 << endl;
	    r = read_format_error;
	    break;
	}

	for (j=0; j < num_trans; j++)
	{
	    in_sym = get_int(fd,swap);
	    if (in_sym < 0)
	    {
		in_sym *= -1;
		out_sym = in_sym;
	    }
	    else
		out_sym = get_int(fd,swap);
	    next_state = get_int(fd,swap);
	    trans_cost = get_float(fd,swap);

	    p_states[i]->add_transition(trans_cost,next_state,in_sym,out_sym);
	}
    }

    return r;
}


EST_read_status EST_WFST::load(const EST_String &filename)
{
    // Load a WFST from a file
    FILE *fd;
    EST_TokenStream ts;
    EST_Option hinfo;
    bool ascii;
    EST_EstFileType t;
    EST_read_status r;
    int i,s;
    int swap;

    if ((fd=fopen(filename,"r")) == NULL)
    {
	cerr << "WFST load: unable to open \"" << filename 
	    << "\" for reading" << endl;
	return read_error;
    }
    ts.open(fd,FALSE);
    ts.set_quotes('"','\\');

    if (((r = read_est_header(ts, hinfo, ascii, t)) != format_ok) ||
	(t != est_file_fst))
    {
	cerr << "WFST load: not a WFST file \"" << filename << "\"" <<endl;
	return misc_read_error;
    }

    // Value is a quoted quoted s-expression.  Two reads is the 
    // safest way to unquote them
    LISP inalpha = 
	read_from_string(get_c_string(read_from_string(hinfo.val("in"))));
    LISP outalpha = 
	read_from_string(get_c_string(read_from_string(hinfo.val("out"))));
    p_start_state = 0;

    clear();
    init(inalpha,outalpha);

    int num_states = hinfo.ival("NumStates");
    r = format_ok;

    if (!ascii)
    {
	if (!hinfo.present("ByteOrder"))
	    swap = FALSE;  // ascii or not there for some reason
	else if (((hinfo.val("ByteOrder") == "01") ? bo_little : bo_big) 
		 != EST_NATIVE_BO)
	    swap = TRUE;
	else
	    swap = FALSE;
	r = load_binary(fd,hinfo,num_states,swap);
    }
    else
    {
	for (i=0; i < num_states; i++)
	{
	    LISP sd = lreadf(fd);
	    if (i != get_c_int(car(car(sd))))
	    {
		cerr << "WFST load: expected description of state " << i <<
		    " but found \"" << siod_sprint(sd) << "\"" << endl;
		r = read_format_error;
		break;
	    }
	    if (streq("final",get_c_string(car(cdr(car(sd))))))
		s = add_state(wfst_final);
	    else if (streq("nonfinal",get_c_string(car(cdr(car(sd))))))
		s = add_state(wfst_nonfinal);
	    else if (streq("licence",get_c_string(car(cdr(car(sd))))))
		s = add_state(wfst_licence);
	    else
	    {
		cerr << "WFST load: unknown state type \"" << 
		    siod_sprint(car(cdr(car(sd)))) << "\"" << endl;
		r = read_format_error;
		break;
	    }
	    
	    if (s != i)
	    {
		cerr << "WFST load: internal error: unexpected state misalignment"
		     << endl;
		r = read_format_error;
		break;
	    }
	    if (load_transitions_from_lisp(s,cdr(sd)) != format_ok)
	    {
		r = read_format_error;
		break;
	    }
	}
    }

    fclose(fd);
    
    return r;
}

EST_read_status EST_WFST::load_transitions_from_lisp(int s, LISP trans)
{
    LISP t;

    for (t=trans; t != NIL; t=cdr(t))
    {
	float w = get_c_float(siod_nth(3,car(t)));
	int end = get_c_int(siod_nth(2,car(t)));
	int in = p_in_symbols.name(get_c_string(siod_nth(0,car(t))));
	int out = p_out_symbols.name(get_c_string(siod_nth(1,car(t))));

	if ((in == -1) || (out == -1))
	{
	    cerr << "WFST load: unknown vocabulary in state transition" 
		<< endl;
	    cerr << "WFST load:  " << siod_sprint(car(t)) << endl;
	    return read_format_error;
	}
	p_states[s]->add_transition(w,end,in,out);
    }
    return format_ok;
}

EST_String EST_WFST::summary() const
{
    int i;
    int tt=0;

    for (i=0; i < p_num_states; i++)
	tt += p_states(i)->transitions.length();

    return EST_String("WFST ")+itoString(p_num_states)+" states "+
	itoString(tt)+" transitions ";
}
    

void EST_WFST::more_states(int new_max)
{
    int i;

    p_states.resize(new_max);
    for (i=p_num_states; i < new_max; i++)
	p_states[i] = 0;
}

int EST_WFST::add_state(enum wfst_state_type state_type)
{
    // Add new state of given type
    EST_WFST_State *s = new EST_WFST_State(p_num_states);

    if (p_num_states >= p_states.length())
    {
	// Need more space for states
	more_states((int)((float)(p_states.length()+1)*1.5));
    }

    p_states[p_num_states] = s;
    s->set_type(state_type);
    p_num_states++;

    return s->name();
}

void EST_WFST::start_cumulate(void)
{
    // cumulate transitions during recognition
    EST_Litem *j;
    int i;

    p_cumulate = 1;
    for (i=0; i < p_num_states; i++)
    {
	EST_WFST_State *s=p_states[i];
	for (j=s->transitions.head(); j !=0; j=j->next())
	    s->transitions(j)->set_weight(0);
    }
}

void EST_WFST::stop_cumulate(void)
{
    EST_Litem *j;
    int i;
    float t;

    p_cumulate = 0;
    for (i=0; i < p_num_states; i++)
    {
	EST_WFST_State *s=p_states[i];
	for (t=0,j=s->transitions.head(); j !=0; j=j->next())
	    t += s->transitions(j)->weight();
	if (t > 0)
	    for (j=s->transitions.head(); j !=0; j=j->next())
		s->transitions(j)->set_weight(s->transitions(j)->weight()/t);
    }
}
