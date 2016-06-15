/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1995,1996                          */
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
/*                    Author :  Simon King                               */
/*                    Date   :  November 1996                            */
/*-----------------------------------------------------------------------*/
/*                   Lattice/Finite State Network                        */
/*                                                                       */
/*=======================================================================*/

#include "EST_lattice.h"
#include <fstream>
#include <cstdlib>

Lattice::Lattice()
{
    tf=NULL;
}


Lattice::~Lattice()
{

}

Lattice::Node*
Lattice::start_node()
{
    if(nodes.head() != NULL )
	return nodes(nodes.head()); 
    else{
	cerr << "LAttice has no nodes !" << endl;
	return NULL;
    }

}

#if 0
bool Lattice::build_qmap(Bigram &g, float error_margin)
{

    // very crude VQ (not vectors though)
    // works best on log bigrams ?

    // to do : automatically determine error_margin

    int i,j;
    EST_Litem *l_ptr;
    bool flag;

    // first, build a list, then transfer to array
    Tlist<float> list_qmap;

    qmap_error_margin = error_margin;

    for (i = 0; i < g.size(); ++i){
	for (j = 0; j < g.size(); ++j){
		
	    flag = false;
	    for(l_ptr=list_qmap.head();l_ptr != 0; l_ptr=l_ptr->next())
		if(fabs(list_qmap(l_ptr) - g.p(i,j)) <= error_margin){
		    flag = true;
		    break;
		}

	    if(!flag)
		list_qmap.append(g.p(i,j));
	    
	}
    }

    // special zero (within error_margin) entry, if not already there
    flag = false;
    for(l_ptr=list_qmap.head();l_ptr != 0; l_ptr=l_ptr->next())
	if(fabs(list_qmap(l_ptr)) <= error_margin){
	    flag = true;
	    break;
	}
    
    if(!flag)
	list_qmap.append(0);
    
    qsort(list_qmap);

    i=0;
    for(l_ptr=list_qmap.head();l_ptr != 0; l_ptr=l_ptr->next())
	i++;

    // transfer to array
    qmap.resize(i);
    i=0;
    for(l_ptr=list_qmap.head();l_ptr != 0; l_ptr=l_ptr->next())
	qmap(i++) = list_qmap(l_ptr);

    list_qmap.clear();


    cerr << "Built qmap with " << i << " entries" << endl;

    return true;
}


bool
Lattice::build_nmap(Bigram &g)
{


    int j;
    //name_map_entry_t entry;
    EST_Litem *l_ptr;

    // first, build a list, then transfer to array
    Tlist<EST_String> list_nmap;

    // wordlist must not have !ENTER of !EXIT in it
    for (j = 0; j < g.size() - 2; ++j){
	if(g.words(j) == "!ENTER"){
	    cerr << "Wordlist contains special word !ENTER" << endl;
	    return false;
	}
	if(g.words(j) == "!EXIT"){
	    cerr << "Wordlist contains special word !EXIT" << endl;
	    return false;
	}
    }




    // add all words
    for (j = 0; j < g.size() - 2; ++j)
	list_nmap.append(g.words(j));

    // special enter and exit
    list_nmap.append("!ENTER");
    list_nmap.append("!EXIT");

    qsort(list_nmap);

    cerr << list_nmap << endl;

    j=0;
    for(l_ptr=list_nmap.head();l_ptr != 0; l_ptr=l_ptr->next())
	j++;

    // transfer to array
    nmap.resize(j);
    j=0;
    for(l_ptr=list_nmap.head();l_ptr != 0; l_ptr=l_ptr->next())
	nmap(j++) = list_nmap(l_ptr);

    list_nmap.clear();
    cerr << "Built nmap with " << j << " entries" << endl;

    return true;
}


bool
Lattice::construct_alphabet(Bigram &g)
{

    int i,j,enteri,exiti,aindex,qindex,nindex,count;
    symbol_t *sym;
    EST_Litem *a_ptr;
    bool flag;

    if(!build_qmap(g,1.0e-02) || !build_nmap(g)) // to do : fix
	return false;


    // temporary list
    Tlist<symbol_t*> list_alphabet;

    // alphabet consists of all occurring combinations of
    // nmap and qmap entries

    enteri = g.size() - 2;
    exiti  = g.size() - 1;

    aindex=0;

    // order is this way for speed
    for (j = 0; j < g.size(); ++j){
	
	cerr << "constructing alphabet " << (int)((float)j*100/(float)g.size()) << "%     \r";
	
	if(j == enteri)
	    nindex = nmap_name_to_index("!ENTER");
	else if(j == exiti)
	    nindex = nmap_name_to_index("!EXIT");
	else
	    nindex = nmap_name_to_index(g.words(j)); // check !!
	
	for (i = 0; i < g.size(); ++i){
	    
	    qindex = qmap_value_to_index(g.p(i,j));
		  
	    // have we got sym already ?
	    flag=false;
	    for(a_ptr=list_alphabet.tail();a_ptr!=NULL;a_ptr=a_ptr->prev()){
		if( (list_alphabet(a_ptr)->nmap_index == nindex) &&
		    (list_alphabet(a_ptr)->qmap_index == qindex) ){
		    flag=true;
		    break;
		}
		
		// since words are added in order, stop
		// when we get to previous word
		if(list_alphabet(a_ptr)->nmap_index != nindex)
		    break;
	    }

	    
	    if(!flag){
		sym = new symbol_t;
		sym->qmap_index=qindex;
		sym->nmap_index=nindex;
		list_alphabet.append(sym);
	    }
	}
    }

    // special ENTER with prob of 1 symbol
    nindex = nmap_name_to_index("!ENTER");
    qindex = qmap_value_to_index(0); // log prob
    flag=false;
    for(a_ptr=list_alphabet.tail();a_ptr!=NULL;a_ptr=a_ptr->prev())
	if( (list_alphabet(a_ptr)->nmap_index == nindex) &&
	    (list_alphabet(a_ptr)->qmap_index == qindex) ){
	    flag=true;
	    break;
	}
    
    if(!flag){
	sym = new symbol_t;
	sym->qmap_index=qindex;
	sym->nmap_index=nindex;
	list_alphabet.append(sym);
    }

    // and special no-label label (e-move)
    sym = new symbol_t;
    sym->qmap_index=-1;
    sym->nmap_index=-1;
    list_alphabet.append(sym);

    ptr_qsort(list_alphabet);

    count=0;
    for(a_ptr=list_alphabet.head();a_ptr != NULL; a_ptr=a_ptr->next())
	count++;


    alphabet.resize(count);
    count=0;
    for(a_ptr=list_alphabet.head();a_ptr != NULL; a_ptr=a_ptr->next())
	alphabet(count++) = *(list_alphabet(a_ptr));
    
    // .. to do - delete syms
    list_alphabet.clear();

    e_move_symbol_index = alphabet_index_lookup(-1,-1);

    cerr << "Alphabet has " << count << " symbols                   " << endl;
    
    return true;
}


bool
Lattice::construct(Bigram &g)
{

    // every word becomes a node, every non-zero transition becomes an arc
    // eliminate any transitions into ENTER and out of EXIT
    int i,j,k;
    EST_Litem *n_ptr,*a_ptr;
    Node *n;
    Arc *a;
    Node *from,*to;
    int symbol;
    // unfortunate, but fixed in Bigram class
    int enteri = g.size() - 2;
    int exiti  = g.size() - 1;
    int from_name,to_name;
    
    // single entry node, no name since no arcs in
    // single arc out to node called "!ENTER"

    Node *enter_node = new Node;
    enter_node->name.append(-1); // no name !
    nodes.append(enter_node);

    // make nodes - one for every entry in nmap
    for(i=0; i<nmap.n(); i++){

	n = new Node;
	n->name.append(i); // index into nmap
	nodes.append(n);

	if(nmap(i) == "!ENTER"){ // temporary hack

	    symbol = alphabet_index_lookup(i,qmap_value_to_index(0)); // for log probs !!!
	    a = new Arc;
	    a->label = symbol;
	    a->to = n;
	    enter_node->arcs_out.append(a);

	}


	// keep note of final node
	if(nmap(i) == "!EXIT") // temporary hack
	    final_nodes.append(n);

    }

    // make arcs
    k=0;
    for (i = 0; i < g.size(); ++i){
	cerr << "building lattice " << (int)((float)i*100/(float)g.size()) << "%    \r";
	for (j = 0; j < g.size(); ++j){

	    // ignore any transitions into ENTER or out of EXIT
	    //if((g.p(i, j) > 0) &&  - for probs only, can't do for log probs
	    if((j != enteri) && (i != exiti)){

		// find from and to nodes
		from=NULL;
		to=NULL;

		if(i == enteri)
		    from_name = nmap_name_to_index("!ENTER");
		else
		    from_name = nmap_name_to_index(g.words(i));

		if(j == exiti)
		    to_name = nmap_name_to_index("!EXIT");
		else
		    to_name= nmap_name_to_index(g.words(j));

		for (n_ptr = nodes.head(); n_ptr != 0; n_ptr = n_ptr->next()){
		    int name = nodes(n_ptr)->name(nodes(n_ptr)->name.head());
		    if(name == from_name)
			from = nodes(n_ptr);
		    if(name == to_name)
			to = nodes(n_ptr);
		}

		if(from==NULL){
		    cerr << "Couldn't find 'from' node " << nmap_index_to_name(from_name) << endl;
		    return false;
		}
		    
		if(to==NULL){
		    cerr << "Couldn't find 'to' node " << nmap_index_to_name(to_name) << endl;
		    return false;
		}

		// get arc symbol - speed up this fn !
		symbol = alphabet_index_lookup(to_name,qmap_value_to_index(g.p(i,j)));
		if(symbol < 0){
		    cerr << "Couldn't lookup symbol in alphabet !" << endl;
		    return false;
		}
 	
		a = new Arc;
		a->label = symbol;
		a->to = to;
		
		from->arcs_out.append(a);
		

	    }
	}
    }
    cerr << "                                    \r";

    int c1=0,c2=0,c3=0;
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	c1++;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    c2++;
    }
    
    for (n_ptr = final_nodes.head(); n_ptr != 0; n_ptr = n_ptr->next())
	c3++;

    cerr << "NFA has " << c1 
	 << " nodes (" << c3 << " final)"
	 << " and " << c2 << " arcs" << endl;
    
    return true;
}
#endif

bool Lattice::determinise()
{
    // very simple, but potentially time/memory consuming

    // make a new lattice whose nodes are all permutations
    // of the nodes in the old lattice
    // BUT : only make the nodes which are actually required
    // (otherwise number of new nodes = 2^(number of old nodes)

    // we will call the old lattice NFA and the new one DFA

    EST_Litem *n_ptr,*n2_ptr,*a_ptr,*l_ptr;
    Node *new_node;
    EST_TList<Node*> new_nodes;
    EST_TList<Node*> new_final_nodes;
    EST_TList<Arc*> new_arcs;
    EST_TList<Arc*> arc_list;
    EST_TList<int> new_name;
    int c,count,current_label,new_arcs_made;
    bool is_final;

    // first, sort nodes' lists of arcs by label to make 
    // it easier
    cerr << "sorting arc lists" << endl;
    sort_arc_lists();

    cerr << "making new nodes" << endl;

    // - for every node in the NFA, look at the outgoing arcs with
    //   the same label
    // - make a DFA node which is a combination of the NFA nodes
    //   which are the destinations of those arcs

    // there are more DFA nodes made later

    // enter node is special case, it has no in-going arcs
    // so will not be made in the following loop

    // for now, assume one enter node at head of list ... hmmmmm
    new_node = new Node;
    if(new_node == NULL){
	cerr << "Could not allocate any more memory";
	return false;
    }
    new_node->name = nodes(nodes.head())->name;
    new_nodes.append(new_node);



    for (n_ptr = nodes.head(); n_ptr != 0; n_ptr = n_ptr->next()){

	for (a_ptr = nodes(n_ptr)->arcs_out.head(); a_ptr != 0; a_ptr = a_ptr->next()){

	    current_label = nodes(n_ptr)->arcs_out(a_ptr)->label;

	    // make list of arc destinations along arcs with this label
	    is_final=false;
	    new_name.clear();
	    new_name = nodes(n_ptr)->arcs_out(a_ptr)->to->name;
	    if(final(nodes(n_ptr)->arcs_out(a_ptr)->to) )
		is_final=true;
	    while((a_ptr != NULL) &&
		  (a_ptr->next() != NULL) &&
		  (nodes(n_ptr)->arcs_out(a_ptr->next())->label == current_label)){
		merge_sort_unique(new_name,nodes(n_ptr)->arcs_out(a_ptr->next())->to->name);

		if( !is_final && final(nodes(n_ptr)->arcs_out(a_ptr->next())->to) )
		    is_final=true;

		a_ptr=a_ptr->next();

	    }

	    // make new node with this name, unless we already have one
	    bool flag=false;
	    for (n2_ptr = new_nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next())
		if( new_nodes(n2_ptr)->name == new_name ){
		    flag=true;
		    break;
		}
	    
	    if(!flag){ // need to make new node

		new_node = new Node;
		if(new_node == NULL){
		    cerr << "Could not allocate any more memory";
		    count=0;
		    for (n2_ptr = new_nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next())
			count++;
		    cerr << " after making " << count << " nodes" << endl;
		    return false;
		}

		new_node->name = new_name;
		new_nodes.append(new_node);

		if(is_final)
		    new_final_nodes.append(new_node);
	    }

	}  // loop round outgoing arcs for this node
	
    } // loop round NFA nodes
    

    // now, for every node in the DFA, its 'transition function' (i.e. outgoing arcs)
    // is the sum of the transition functions of its constituent nodes from
    // the NFA

    c=0;
    new_arcs_made=0;
    for (n_ptr = new_nodes.head(); n_ptr != 0; n_ptr = n_ptr->next()){

	c++;
	cerr << "Processing node " << c
	     << "  arcs=" << new_arcs_made << "       \r";

	// find constituent nodes in NFA (only works if NFA names are single ints !)
	arc_list.clear();
	for(l_ptr=new_nodes(n_ptr)->name.head(); l_ptr != 0; l_ptr = l_ptr->next()){

	    for(n2_ptr = nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next()){
		
		if(nodes(n2_ptr)->name(nodes(n2_ptr)->name.head()) ==
		   new_nodes(n_ptr)->name(l_ptr))
 		    arc_list += nodes(n2_ptr)->arcs_out;


	    }
	}

	// now we have a list of all the NFA nodes we can 'get to'
	// from this DFA node, sort by label and find the
	// DFA node for each label

	// or rather, take the arc list from above, and collapse
	// all arcs with the same label into one arc to the DFA
	// node which is the equivalent of the NFA nodes at the
	// end of those arcs

	sort_by_label(arc_list);

	for(a_ptr=arc_list.head();a_ptr!=NULL;a_ptr=a_ptr->next()){

	    current_label=arc_list(a_ptr)->label;
	    //cerr << " label " << current_label;

	    is_final=false;
	    EST_TList<int> new_name2;
	    new_name2 = arc_list(a_ptr)->to->name;
	    if(final(arc_list(a_ptr)->to) )
		is_final=true;
	    while((a_ptr != NULL) &&
		  (a_ptr->next() != NULL) &&
		  (arc_list(a_ptr->next())->label == current_label)){
		merge_sort_unique(new_name2,arc_list(a_ptr)->to->name);
		if( !is_final && final(arc_list(a_ptr)->to) )
		    is_final=true;
		a_ptr=a_ptr->next();
	    }

	    //cerr << " -> " << new_name2;

	    // find DFA node with that name
	    bool flag=false;
	    for (n2_ptr = new_nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next())
		if( new_nodes(n2_ptr)->name == new_name2 ){
		    flag=true;
		    break;
		}
	    
	    if(!flag){ // failed to make node previously, do it now
		new_node = new Node;
		if(new_node == NULL){
		    cerr << "Could not allocate any more memory";
		    count=0;
		    for (n2_ptr = new_nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next())
			count++;
		    cerr << " after making " << count << " nodes" << endl;
		    return false;
		}

		new_node->name = new_name2;
		new_nodes.append(new_node);
		link(new_nodes(n_ptr),new_node,current_label);

		if(is_final)
		    new_final_nodes.append(new_node);

	    }else{

		link(new_nodes(n_ptr),new_nodes(n2_ptr),current_label);

	    }

		new_arcs_made++;
	}


    } // loop round DFA nodes

    cerr << endl;

    // delete old nodes, and replace with new nodes
    for (n_ptr = nodes.head(); n_ptr != 0; n_ptr = n_ptr->next())
	delete nodes(n_ptr);
    nodes.clear();
    nodes=new_nodes;
    new_nodes.clear();

    final_nodes.clear();
    final_nodes=new_final_nodes;
    new_final_nodes.clear();

    int c1=0,c2=0,c3=0;
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	c1++;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    c2++;
    }
    
    for (n_ptr = final_nodes.head(); n_ptr != 0; n_ptr = n_ptr->next())
	c3++;

    cerr << "DFA has " << c1 
	 << " nodes (" << c3 << " final)"
	<< " and " << c2 << " arcs" << endl;


    return true;
}

bool
Lattice::link(Node *n1, Node *n2, int label)
{

    //if(l==NULL)
    //l=&arcs;

    // create arc between two nodes
    // in direction n1,n2 
    Arc *new_arc;

    if( (n1==NULL) || (n2==NULL) ){
	cerr << "Can't link null nodes" << endl;
	return false;
    }
	


    new_arc = new Arc;
    //new_arc->from = n1;
    new_arc->to = n2;
    new_arc->label = label;

    n1->arcs_out.append(new_arc);
    //n2->arcs_in.append(new_arc);
    //l->append(new_arc);

    return true;
}

void
Lattice::sort_arc_lists()
{
    EST_Litem *n_ptr;

    for (n_ptr = nodes.head(); n_ptr != 0; n_ptr = n_ptr->next()){
	
	// can't sort nodes(n_ptr)->arcs_out directly
	// since it is a list of pointers

	sort_by_label(nodes(n_ptr)->arcs_out);

    }

}

void
sort_by_label(EST_TList<Lattice::Arc*> &l){
    ptr_qsort(l);
}


bool
Lattice::build_distinguished_state_table(bool ** &dst)
{

    int i,j;
    EST_Litem *n_ptr,*n2_ptr;

    int num_nodes = nodes.length();
    // not every element will be used
    dst = new bool*[num_nodes];
    if(dst==NULL){
	cerr << "Not enough memory" << endl;
	return false;
    }
    for(i=0;i<num_nodes;i++){
	dst[i] = new bool[num_nodes];
	if(dst[i]==NULL){
	    cerr << "Not enough memory" << endl;
	    return false;
	}
	for(j=0;j<num_nodes;j++)
	    dst[i][j] = false;

    }

    cerr << "final/non-final scan";
    // any final/non-final (or non-final/final) pairs are distinguished immediately
    for(i=0,n_ptr=nodes.head();n_ptr->next()!=NULL;n_ptr=n_ptr->next(),i++){
	for(j=i+1,n2_ptr=n_ptr->next();n2_ptr!=NULL;n2_ptr=n2_ptr->next(),j++){
	    if(final(nodes(n_ptr)) && !final(nodes(n2_ptr)))
		dst[i][j] = true;
	    else if(!final(nodes(n_ptr)) && final(nodes(n2_ptr)))
		dst[i][j] = true;
	    
	}
    }
    cerr << "\r                        \r";

    // two possible methods, depending on network representation
    // for sparse networks, use actual nodes and their lists of arcs
    //build_distinguished_state_table_direct(dst);
    
    // for dense networks, use a transition function matrix
    // which will be faster but more memory consuming

    if(!build_transition_function()){
	cerr << "Couldn't build transition function" << endl;
	return false;
    }

    if(!build_distinguished_state_table_from_transition_function(dst)){
	cerr << "Couldn't build dst from transition function" << endl;
	return false;
    }

    // delete tf, since it will be wrong for minimised net
    for(i=0;i<num_nodes;i++)
	delete [] tf[i];
    delete [] tf;
    tf=NULL;

    return true;
}

bool
Lattice::build_distinguished_state_table_direct(bool ** &dst)
{

    int i,j,i1,j1,scan_count,this_symbol;
    EST_Litem *n_ptr,*n2_ptr,*a_ptr,*s_ptr;
    bool flag,flag2;

    // carry out repeated scans of the distinguished state table until no more
    // cells are set true

    flag=true;
    scan_count=0;
    while(flag){
	flag=false;

	scan_count++;

	for(i=0,n_ptr=nodes.head();n_ptr->next()!=NULL;n_ptr=n_ptr->next(),i++){
	    for(j=i+1,n2_ptr=n_ptr->next();n2_ptr!=NULL;n2_ptr=n2_ptr->next(),j++){
		cerr << "scan " << scan_count << " : " << i << "," << j << "     \r";

		if(!dst[i][j]){

		    // go through alphabet, or rather just those symbols
		    // occurring on the arcs out of this pair of nodes
		    
		    flag2=true;
		    s_ptr=nodes(n_ptr)->arcs_out.head();
		    while(s_ptr != NULL){


			if(flag2){ // we are going through symbols on arcs out of 'nodes(n_ptr)'
			    this_symbol=nodes(n_ptr)->arcs_out(s_ptr)->label;
			    i1 = node_index(nodes(n_ptr)->arcs_out(s_ptr)->to);

			    j1=-1;
			    for(a_ptr=nodes(n2_ptr)->arcs_out.head();
				a_ptr!=NULL; a_ptr=a_ptr->next())
				if(nodes(n2_ptr)->arcs_out(a_ptr)->label == this_symbol)
				    j1 = node_index(nodes(n2_ptr)->arcs_out(a_ptr)->to);

			}else{ // we are going through symbols on arcs out of 'nodes(n2_ptr)'
			    this_symbol=nodes(n2_ptr)->arcs_out(s_ptr)->label;
			    j1 = node_index(nodes(n2_ptr)->arcs_out(s_ptr)->to);

			    i1=-1;
			    for(a_ptr=nodes(n_ptr)->arcs_out.head();
				a_ptr!=NULL; a_ptr=a_ptr->next())
				if(nodes(n_ptr)->arcs_out(a_ptr)->label == this_symbol)
				    i1 = node_index(nodes(n_ptr)->arcs_out(a_ptr)->to);
			}


			// States (nodes) are distinguished if, for any symbol, they
			// have transitions (arcs) to a pair of distinguished states.
			// This includes the case where, for any symbol, one state
			// has a transition and the other does not

			if(   ( (i1>=0) && (j1>=0) && dst[i1][j1]) ||
			      ( (i1>=0) && (j1<0) ) ||
			      ( (j1>=0) && (i1<0) )     )
			    {
				dst[i][j] = true;
				flag=true;
				break; // out of s_ptr loop
				
			    }

			s_ptr=s_ptr->next();
			if( (s_ptr==NULL) && (flag2) ){ // now go down second list
			    s_ptr=nodes(n2_ptr)->arcs_out.head();
			    flag2=false;
			}
			
		    }
		}
	    }
	}
    }

    return true;
}

bool
Lattice::build_distinguished_state_table_from_transition_function(bool ** &dst)
{
    int i,j,i2,j2,k,scan_count;
    int num_nodes = nodes.length();
    int num_symbols = alphabet.n();
    bool flag;

    flag=true;
    scan_count=0;
    while(flag){
	flag=false;
	
	scan_count++;
	
	for(i=0;i<num_nodes-1;i++){

	    cerr << "scan " << scan_count << " : row "
		 << i << "   \r";
	    
	    for(j=i+1;j<num_nodes;j++){
		
		if( !dst[i][j] ){
		    
		    for(k=0;k<num_symbols;k++){
			
			i2 = tf[i][k];
			j2 = tf[j][k];
			
			if((i2<0) && (j2>=0)){
			    dst[i][j] = true;
			    flag=true;
			    break;

			}else if((j2<0) && (i2>=0)){
			    dst[i][j] = true;
			    flag=true;
			    break;
			    
			}else if( (i2>0) && (j2>0) && dst[i2][j2] ){
			    dst[i][j] = true;
			    flag=true;
			    break;
			}
			
		    }
		}
	    }
	}	    
    }

    return true;
}

bool
Lattice::minimise()
{

    // method, make distinguished state table
    // scan all pairs of states (a.k.a. nodes)


    int i,j;
    EST_Litem *n_ptr,*n2_ptr,*n3_ptr,*a_ptr;
    int num_nodes = nodes.length();
    bool **dst = NULL; // distinguished state table
    bool flag;

    if(!build_distinguished_state_table(dst)){
	cerr << "Couldn't build distinguished state table" << endl;
	return false;
    }

    int count=0,count2=0;
    for(i=0;i<num_nodes-1;i++)
	for(j=i+1;j<num_nodes;j++)
	    if(!dst[i][j])
		count++;
	    else
		count2++;
    
    cerr << "There are " << count << " undistinguished pairs of nodes and "
	 << count2 << " distinguished pairs" << endl;


    // make lists of nodes to be merged
    EST_TList<Node *> merge_list;
	


    flag=true;
    while(flag){
	flag=false;

	merge_list.clear();
	for(i=0,n_ptr=nodes.head();n_ptr->next()!=NULL;n_ptr=n_ptr->next(),i++){

	    cerr << "merge, processing row " << i << "        \r";
	    
	    for(j=i+1,n2_ptr=n_ptr->next();n2_ptr!=NULL;n2_ptr=n2_ptr->next(),j++){
		
		if(!dst[i][j]){

		    // is the merge list empty ?
		    if(merge_list.head() == NULL){
			// put this pair of nodes on merge list
			merge_list.append(nodes(n_ptr));
			merge_list.append(nodes(n2_ptr));
			dst[i][j] = true;

		    }else{ // merge list already has some items on it

			// see if either of this pair of nodes is on the merge list,
			// and if so add the other node in the pair
			bool add1=false,add2=false;
			for(n3_ptr=merge_list.head();n3_ptr!=NULL;n3_ptr=n3_ptr->next()){
			    if(merge_list(n3_ptr) == nodes(n_ptr))
				add2=true;
			    if(merge_list(n3_ptr) == nodes(n2_ptr))
				add1=true;
			}
			

			if(add1 && !add2){
			    merge_list.append(nodes(n_ptr));
			    dst[i][j] = true;
			}else if(add2 && !add1){
			    merge_list.append(nodes(n2_ptr));
			    dst[i][j] = true;
			}
			

		    }
		}
	    }
	}

	// anything on the merge list ?
	if(merge_list.head() != NULL){

	    // so merge them
	    count=0;
	    for(n_ptr=merge_list.head();n_ptr!=NULL;n_ptr=n_ptr->next())
		count++;
	    cerr << "merging " << count << " nodes out of ";
	    
	    count=0;
	    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next())
		count++;
	    cerr << count;
	    
	    merge_nodes(merge_list);
	    flag=true;

	    count=0;
	    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next())
		count++;
	    cerr << " leaving " << count << endl;
	    
	    
	}


    }

    int c1=0,c2=0;
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	c1++;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    c2++;
    }
    
    cerr << "Minimum state DFA has " << c1 
	 << " nodes and " << c2 << " arcs         " << endl;

    merge_arcs();

    c1=0,c2=0;
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	c1++;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    c2++;
    }
    
    cerr << "Pruned minimum state DFA has " << c1 
	 << " nodes and " << c2 << " arcs" << endl;

    for(i=0;i<num_nodes;i++)
	delete [] dst[i];
    delete [] dst;
    
    return true;
}


void
Lattice::merge_nodes(EST_TList<Node*> &l)
{

    if(l.head() == NULL)
	return;

    // make a new node with all node labels of old nodes
    // and all arcs of old nodes

    EST_Litem *n_ptr,*n2_ptr,*a_ptr,*a2_ptr;
    Node *new_node;
    new_node = new Node;


    // to do .. deal with final_nodes list too ....



    for(n_ptr=l.head();n_ptr!=NULL;n_ptr=n_ptr->next()){

	// get arcs
	for(a_ptr=l(n_ptr)->arcs_out.head();a_ptr!=NULL;a_ptr=a_ptr->next())
	    new_node->arcs_out.append(l(n_ptr)->arcs_out(a_ptr));

	// get name
	merge_sort_unique(new_node->name,l(n_ptr)->name);

	// find arcs into old nodes and make them go into new node
	for(n2_ptr=nodes.head();n2_ptr!=NULL;n2_ptr=n2_ptr->next()){
	    for(a2_ptr=nodes(n2_ptr)->arcs_out.head();a2_ptr!=NULL;a2_ptr=a2_ptr->next()){
		if(nodes(n2_ptr)->arcs_out(a2_ptr)->to == l(n_ptr))
		    nodes(n2_ptr)->arcs_out(a2_ptr)->to = new_node;
	    }
	}

    }


    // delete old nodes, but not arcs
    for(n_ptr=l.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	for(n2_ptr=nodes.head();n2_ptr!=NULL;n2_ptr=n2_ptr->next())
	    if(nodes(n2_ptr) == l(n_ptr)){
		nodes(n2_ptr)->name.clear();
		nodes(n2_ptr)->arcs_out.clear();
		delete nodes(n2_ptr);
		nodes.remove(n2_ptr);
	    }
    }
    
    nodes.append(new_node);

}

void
Lattice::merge_arcs()
{

    EST_Litem *n_ptr,*a_ptr,*a2_ptr;
    EST_TList<Arc*> merge_list;
    int count=0,count2;

    // find repeated arcs with the same label between two nodes
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){

	count2=0;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    count2++;

	cerr << "merging arcs from node " << ++count 
	     << ", before:" << count2;

	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr->next()!=NULL; a_ptr=a_ptr->next()){
	    
	    merge_list.clear();
	    for(a2_ptr=a_ptr->next();a2_ptr!=NULL; a2_ptr=a2_ptr->next())

		if((nodes(n_ptr)->arcs_out(a_ptr)->label ==
		   nodes(n_ptr)->arcs_out(a2_ptr)->label) &&

		    (nodes(n_ptr)->arcs_out(a_ptr)->to ==
		     nodes(n_ptr)->arcs_out(a2_ptr)->to) ){

		    delete nodes(n_ptr)->arcs_out(a2_ptr);
		    a2_ptr=nodes(n_ptr)->arcs_out.remove(a2_ptr);
		    
		}

	}

	count2=0;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    count2++;

	cerr<< ", after:" << count2 << endl;

    }

    cerr << "                                                    \r" << endl;
    
}


void
Lattice::prune_arcs(Node *node, EST_TList<Arc*> arcs)
{

    int count=0;
    EST_Litem *a_ptr;
    for(a_ptr=arcs.head(); a_ptr != 0; a_ptr=a_ptr->next() ){
	prune_arc(node, arcs(a_ptr));
	count++;
    }
    //cerr << "pruned " << count << " arcs" << endl;
}


void
Lattice::prune_arc(Node *node, Arc *arc)
{
    remove_arc_from_nodes_out_list(node,arc);
    delete arc;
}

/*
void
Lattice::remove_arc_from_nodes_in_list(Node *n, Arc *a)
{
    EST_Litem *a_ptr;
    for (a_ptr = n->arcs_in.head(); a_ptr != 0; a_ptr = a_ptr->next())
	if(n->arcs_in(a_ptr) == a)
	    n->arcs_in.remove(a_ptr);
}
*/

void
Lattice::remove_arc_from_nodes_out_list(Node *n, Arc *a)
{
    EST_Litem *a_ptr;
    for (a_ptr = n->arcs_out.head(); a_ptr != 0; a_ptr = a_ptr->next())
	if(n->arcs_out(a_ptr) == a)
	    n->arcs_out.remove(a_ptr);
}

/* not used
bool
Lattice::is_enter_node(Node *n)
{
    // contains "!ENTER" in its list of names
    EST_Litem *l_ptr;
    for(l_ptr=n->name.head(); l_ptr != 0; l_ptr=l_ptr->next())
	if(n->name(l_ptr) == nmap_name_to_index("!ENTER")) // temporary !!
	    return true;
    return false;
}
*/

/* Superseded now we have list of final nodes
bool
Lattice::is_exit_node(Node *n)
{
    EST_Litem *l_ptr;
    for(l_ptr=n->name.head(); l_ptr != 0; l_ptr=l_ptr->next())
	if(n->name(l_ptr) == nmap_name_to_index("!EXIT")) // temporary !!
	    return true;
    return false;
}
*/

EST_String 
Lattice::nmap_index_to_name(int index)
{
    if(index < nmap.n())
	return nmap(index);
    else{
	cerr << "Warning : nmap index " << index << " out of range" << endl;
	return EST_String("!error!");
    }

}

int 
Lattice::nmap_name_to_index(const EST_String &name)
{
    int i,j,mid;

    // binary search
    i=0;
    j=nmap.n()-1;
    
    while(true){
	
	mid = (i+j)/2;

	if(name > nmap(mid))
	   i = mid;
	
	else if(name < nmap(mid))
	    j = mid;
	
	else{ // name == nmap(mid)
	    return mid; // lucky strike
	}

	if(i==j){
	    if(name == nmap(i))
		return i;
	    else{
		cerr << "Lattice::nmap_name_to_index failed for '"
		     << name << "'" << endl;
		return -1;
	    }
	
	}else if(j==i+1){
	    
	    if(name == nmap(i))
		return i;
	    else if(name == nmap(j))
		return j;
	    else{
		cerr << "Lattice::nmap_name_to_index failed for '"
		     << name << "'" << endl;
		return -1;
	    }
	    
	}
	
    }

    return -1;
}


float 
Lattice::qmap_index_to_value(int index)
{
    if(index < qmap.n())
	return qmap(index);
    else{
	cerr << "Warning : qmap index " << index << " out of range" << endl;
	return 1;
    }
}


int
Lattice::qmap_value_to_index(float value)
{
    int i,j,mid;

    // binary search
    i=0;
    j=qmap.n()-1;
    
    while(true){
	
	mid = (i+j)/2;

	if(value > qmap(mid))
	   i = mid;
	
	else if(value < qmap(mid))
	    j = mid;
	
	else
	    return mid; // lucky strike

	if(i==j)
	    return i;

	else if(j==i+1){
	    
	    if( fabs(qmap(i) - value) < fabs(qmap(j) - value) )
		return i;
	    else
		return j;
	}
	
    }
    return 0;
}

int
Lattice::node_index(Node *n)
{
    EST_Litem *n_ptr;
    for (n_ptr = nodes.head(); n_ptr != 0; n_ptr = n_ptr->next()){
	if(nodes(n_ptr) == n)
	    return nodes.index(n_ptr);
	
    }

    //cerr << "Lattice::node_index(Node *n) couldn't find index of node " << n->name;

    return -1;
}




bool
Lattice::expand()
{

    // keep HTK happy - it can't handle multiple arcs into a node
    // with different word labels


    EST_Litem *n_ptr,*n2_ptr,*a_ptr,*w_ptr;
    int word;
    EST_TList<int> word_list;
    Node *new_node;
    Arc *new_arc;
    for (n_ptr = nodes.head(); n_ptr != 0; n_ptr = n_ptr->next()){

	// find all arcs into this node
	word_list.clear();
	for (n2_ptr = nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next()){

	    for (a_ptr = nodes(n2_ptr)->arcs_out.head(); a_ptr != 0; a_ptr = a_ptr->next())
		if(nodes(n2_ptr)->arcs_out(a_ptr)->to == nodes(n_ptr)){
		    
		    if(nodes(n2_ptr)->arcs_out(a_ptr)->label != e_move_symbol_index){
			word = alphabet_index_to_symbol(nodes(n2_ptr)->arcs_out(a_ptr)->label)->nmap_index;
			word_list.append(word);
			sort_unique(word_list);
			
		    }
		}
	}
	
	
	// if word_list.head() is NULL we should be worried
	if((word_list.head() != NULL) && word_list.head()->next() != NULL){

	    // for each word make a null node, change all offending arcs to point
	    // to it, and make another link from null node to nodes(n_ptr)

	    for(w_ptr=word_list.head();w_ptr!=NULL;w_ptr=w_ptr->next()){

		// make null node
		new_node = new Node;
		new_arc = new Arc;
		new_arc->label = e_move_symbol_index; // no label
		new_arc->to = nodes(n_ptr);
		new_node->arcs_out.append(new_arc);

		// for every arc into nodes(n_ptr) with this word label, change its arcs
		for (n2_ptr = nodes.head(); n2_ptr != 0; n2_ptr = n2_ptr->next()){
		    
		    for (a_ptr = nodes(n2_ptr)->arcs_out.head(); a_ptr != 0; a_ptr = a_ptr->next()){
			if(nodes(n2_ptr)->arcs_out(a_ptr)->to == nodes(n_ptr)){
			    
			    word = alphabet_index_to_symbol(nodes(n2_ptr)->arcs_out(a_ptr)->label)->nmap_index;
			    if(word == word_list(w_ptr)){
				
				// change the arc
				nodes(n2_ptr)->arcs_out(a_ptr)->to = new_node;
			    }
			    
			}
		    }
		}
		nodes.append(new_node);
	    }
	}
    }

    // need to make sure ENTER node has no arcs in - if so add a dummy ENTER node

    //Node *enter_node = nodes(nodes.head());
    bool flag=false;
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next()){
	    flag=true;
	    break;
	}
    }

/*
    if(flag){
	cerr << " fixing ENTER node" << endl;
	new_node = new Node;
	new_arc = new Arc;
	new_arc->label = enter_node->label; 
	new_arc->to = enter_node;
	new_node->arcs_out.append(new_arc);
	nodes.append(new_node);
    }	
    */

    // also need to make sure there is only one EXIT node
    if(final_nodes.length() > 1){
	cerr << " making single EXIT node" << endl;
	
	// make null node
	new_node = new Node;

	for(n_ptr=final_nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){

	    new_arc = new Arc;
	    new_arc->label = e_move_symbol_index; // no label
	    new_arc->to = final_nodes(n_ptr);
	    final_nodes(n_ptr)->arcs_out.append(new_arc);


	}

	// empty final node list (nodes themselves remain on nodes list)
	final_nodes.clear();

	// now a single final node
	nodes.append(new_node);
	final_nodes.append(new_node);
    }


    int c1=0,c2=0;
    for(n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next()){
	c1++;
	for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL; a_ptr=a_ptr->next())
	    c2++;
    }
    
    cerr << "HTKified DFA has " << c1 
	 << " nodes and " << c2 << " arcs" << endl;

    return true;
}

bool
Lattice::final(Node *n)
{
    EST_Litem *n_ptr;
    for (n_ptr = final_nodes.head(); n_ptr != 0; n_ptr = n_ptr->next())
	if(final_nodes(n_ptr) == n)
	    return true;

    return false;
}



EST_String Lattice::name_as_string(EST_IList &l)
{
    EST_String name;
    EST_Litem *l_ptr;
    for(l_ptr=l.head();l_ptr!=NULL;l_ptr=l_ptr->next())
	name+=nmap_index_to_name(l(l_ptr)) + ",";

    return name;
}



int
Lattice::alphabet_index_lookup(int nmap_index, int qmap_index)
{
    symbol_t sym;
    sym.nmap_index = nmap_index;
    sym.qmap_index = qmap_index;

    return alphabet_symbol_to_index(&sym);
}


Lattice::symbol_t* 
Lattice::alphabet_index_to_symbol(int index)
{
    if(index < alphabet.n())
	return &(alphabet[index]);
    else{
	cerr << "Warning : alphabet index " << index << " out of range" << endl;
	return NULL;
    }

}

int
Lattice::alphabet_symbol_to_index(Lattice::symbol_t *sym)
{

    int i,j,mid;

    // binary search
    i=0;
    j=alphabet.n()-1;
    
    while(true){
	
	mid = (i+j)/2;

	if(*sym > alphabet(mid))
	   i = mid;
	
	else if(*sym < alphabet(mid))
	    j = mid;
	
	else // *sym == alphabet(mid)
	    return mid; // lucky strike

	if(i==j){
	    if(*sym == alphabet(i))
		return i;
	    else{
		cerr << "Lattice::alphabet_symbol_to_index failed for '"
		     << *sym << "' 1" << endl;
		return -1;
	    }
	
	}else if(j==i+1){
	    
	    if(*sym == alphabet(i))
		return i;
	    else if(*sym == alphabet(j))
		return j;
	    else{
		cerr << "Lattice::alphabet_symbol_to_index failed for '"
		     << *sym << "' 2" << endl;

		cerr << i << " " << alphabet(i) << endl;
		cerr << j << " " << alphabet(j) << endl;

		return -1;
	    }
	    
	}
	
    }

    return -1;

}


bool
Lattice::build_transition_function()
{

    // different representation of the network , currently only for DFAs
    // since for a NFA each tf cell would be a list

    int i,j;
    EST_Litem *n_ptr,*a_ptr;
    int num_nodes = nodes.length();
    int num_symbols = alphabet.n();

    if(tf != NULL)
	cerr << "Warning : discarding existing transition function" << endl;


    tf = new int*[num_nodes];
    for(i=0;i<num_nodes;i++)
	tf[i] = new int[num_symbols];

    if(tf == NULL){
	cerr << "Not enough memory to build transition function"
	     << "(needed " << num_nodes*num_symbols*sizeof(int) << " bytes)" << endl;
	return false;
    }

    for(i=0,n_ptr=nodes.head();n_ptr!=NULL;n_ptr=n_ptr->next(),i++){

	cerr << "building transition function " << (int)((float)(i+1)*100/(float)num_nodes) << "%    \r";

	for(j=0;j<alphabet.n();j++){

	    tf[i][j]=-1; // means no transition
	    for(a_ptr=nodes(n_ptr)->arcs_out.head();a_ptr!=NULL;a_ptr=a_ptr->next()){

		if(j == nodes(n_ptr)->arcs_out(a_ptr)->label){
		    tf[i][j] = node_index(nodes(n_ptr)->arcs_out(a_ptr)->to);
		    break;
		}
	    }
	}
    }

    cerr << endl;
    return true;
}



bool
Lattice::accepts(EST_TList<symbol_t*> &string)
{
    (void) string;
    return false;
}


float
Lattice::viterbi_transduce(EST_TList<EST_String> &input,
			   EST_TList<Arc*> &path,
			   EST_Litem *current_symbol,
			   Node *start_node)
{

    // finds maximum sum-of-probs path
    EST_Litem *a_ptr,*best;

    if(start_node == NULL){
	start_node = nodes(nodes.head());
	current_symbol = input.head();
	path.clear();
    }

    if(current_symbol == NULL){ // consumed all input
	if( final(start_node) )
	    return 0; // log prob 1
	
	else
	    return -10000000; // hack for now
	
    }

    best=NULL;
    float max=-10000000; // hack for now
    for (a_ptr = start_node->arcs_out.head(); a_ptr != 0; a_ptr = a_ptr->next()){

	if( alphabet_index_to_symbol(start_node->arcs_out(a_ptr)->label)->nmap_index
	    == nmap_name_to_index(input(current_symbol)) ){
	    
	    float x = viterbi_transduce(input,
					path,
					current_symbol->next(),
					start_node->arcs_out(a_ptr)->to)
		+ qmap_index_to_value(alphabet_index_to_symbol(start_node->arcs_out(a_ptr)->label)->qmap_index);
	
	    if(x > max){
		max = x;
		best = a_ptr;
	    }
	}
    }

    if(best != NULL)
	path.append(start_node->arcs_out(best));
    
    return max;

}


float Lattice::viterbi_transduce(EST_Track &observations,
			   EST_TList<Arc*> &path,
			   float &score,
			   int current_frame,
			   Node *start_node)
{
    // finds maximum sum-of-probs path
    EST_Litem *a_ptr,*best;

    if(start_node == NULL){
	start_node = nodes(nodes.head());
	current_frame = 0;
	path.clear();
	score = 0.0;
    }
    
    if(current_frame == observations.num_frames()){ // consumed all input
	if( final(start_node) )
	    return 0; // log prob 1
	
	else
	    return -10000000; // hack for now
	
    }
    

    if(score < -100000){ // hack !
	return -10000000;
    }
    
    best=NULL;
    float max=-10000000; // hack for now
    for (a_ptr = start_node->arcs_out.head(); a_ptr != 0; a_ptr = a_ptr->next()){

	int observation_element =
	    alphabet_index_to_symbol(start_node->arcs_out(a_ptr)->label)->nmap_index
	    - 2; // HACK !!@!!!! to skip ENTER/EXIT
	
	float this_observation = observations.a(current_frame,observation_element);
	    
	//cerr << "frame " << current_frame << "," << observation_element
	//<< " : " << this_observation << endl;

	float x = viterbi_transduce(observations,
				    path,
				    score,
				    current_frame+1,
				    start_node->arcs_out(a_ptr)->to)

	    + qmap_index_to_value(alphabet_index_to_symbol(start_node->arcs_out(a_ptr)->label)->qmap_index)
	
	    + this_observation;
	
	if(x > max){
	    max = x;
	    best = a_ptr;
	}

    }

    if(best != NULL){
	path.append(start_node->arcs_out(best));

	int observation_element =
	    alphabet_index_to_symbol(start_node->arcs_out(best)->label)->nmap_index;
	
	float this_observation = observations.a(current_frame,observation_element);

	score += qmap_index_to_value(alphabet_index_to_symbol(start_node->arcs_out(best)->label)->qmap_index) + this_observation;
;

    }

    cerr << max << endl;

    return max;

}

// hack for now (required for SHARED=2)
bool Lattice::symbol_t::operator!=(Lattice::symbol_t const &) const {
return false;
}




