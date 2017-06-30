 ###########################################################################
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1996                            ##
 ##                        All Rights Reserved.                           ##
 ##                                                                       ##
 ##  Permission is hereby granted, free of charge, to use and distribute  ##
 ##  this software and its documentation without restriction, including   ##
 ##  without limitation the rights to use, copy, modify, merge, publish,  ##
 ##  distribute, sublicense, and/or sell copies of this work, and to      ##
 ##  permit persons to whom this work is furnished to do so, subject to   ##
 ##  the following conditions:                                            ##
 ##   1. The code must retain the above copyright notice, this list of    ##
 ##      conditions and the following disclaimer.                         ##
 ##   2. Any modifications must be clearly marked as such.                ##
 ##   3. Original authors' names are not deleted.                         ##
 ##   4. The authors' names are not used to endorse or promote products   ##
 ##      derived from this software without specific prior written        ##
 ##      permission.                                                      ##
 ##                                                                       ##
 ##  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ##
 ##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
 ##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
 ##  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ##
 ##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
 ##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
 ##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
 ##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
 ##  THIS SOFTWARE.                                                       ##
 ##                                                                       ##
 ###########################################################################
 ##                                                                       ##
 ##                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             ##
 ## --------------------------------------------------------------------  ##
 ## Rules for creating documentation.                                     ##
 ##                                                                       ##
 ###########################################################################

MANUALS_GEN=manuals_gen
EXAMPLES_GEN=examples_gen
IMAGES_GEN=images_gen
DOCXX_GEN=doc++
HTML_DIR=$(DOCNAME)


###########################################################################
## User Visable doc++ rule

# This strange idiom is for expensive operations. something dependednt
# on .doc++_made will make doc++ initially, but then not remake it. It
# will notice if doc++ is remade. Something dependent on doc++ will
# remake it every time. The manual pages and examples are also done
# this way.

.doc++file_list_made : 
	@if [ ! -f .doc++file_list_made ] ; then $(MAKE) -C $(TOP)/doc --no-print-directory doc++file_list ; fi

doc++file_list : FORCE
	@echo "Building doc++ files"
	@echo "empty $(TOP)/Doc++Files"
	@$(RM) -f $(TOP)/Doc++Files
	@$(ECHO_N) > $(TOP)/Doc++Files
ifdef DOCXX_DIRS
	@for d in $(DOCXX_DIRS) ; do \
		$(MAKE) -C $$d --no-print-directory doc++files ;\
	done
endif
	@date > .doc++file_list_made

.doc++_made:
	@if [ ! -f .doc++_made ] ; then $(MAKE) -C $(TOP)/doc --no-print-directory doc++ ; fi

doc++: FORCE
	@$(MAKE) -C $(TOP)/doc --no-print-directory doc++file_list doc++process sane_to_docbook
	@date > .doc++_made

doc++process : FORCE
	@echo Clear out doc++ directory
	@$(subst DIR,$(DOCXX_GEN),$(ENSURE_EMPTY_DIR))
	@$(RM) -f .tex_done
ifdef DOCXX_DIRS
	@echo DOC++
	@$(DOCXX) $(DOCXX_ARGS) -d $(DOCXX_GEN) $(shell sed -e 's/^/$(TOP)\//' $(TOP)/Doc++Files) $(DOCXX_EXTRA_FILES)
	@cp -p $(DOCXXIMAGES) $(DOCXX_GEN)
endif

sane_to_docbook: FORCE
	@$(ECHO_N) 'Convert to Docbook: '
	@$(RM) -f $(DOCXX_GEN)/declare_entities.sgml $(DOCXX_GEN)/include_entities.sgml
ifdef DOCXX_DIRS
	@$(MAKE) -C $(TOP)/doc --no-print-directory $(subst sane,db,$(wildcard $(DOCXX_GEN)/*.sane))
endif
	@echo " :done"

$(DOCXX_GEN)/%.db: FORCE
	@{ \
	name=`basename $@ .db` ;\
	idname=`echo "docpp-$$name"|sed -e 's/[^a-zA-Z0-9][^a-zA-Z0-9]*/-/g'` ;\
	sane="$(DOCXX_GEN)/$$name.sane" ;\
	db="$(DOCXX_GEN)/$$name.db" ;\
	$(ECHO_N) "$$name " ;\
	jade -E 2000 -d $(DSSL_SANE_DB) -t sgml $$sane > $$db 2>>$(DOCXX_GEN)/jade_trace ;\
	echo "<!entity $$idname SYSTEM '$$db'>" >&3 ;\
	echo "&$$idname;" >&4 ;\
	for inc in 2 3 4 ;\
		do \
		perl -pe '$$inc='$$inc';s%<(/)?sect([0-9]+)%"<".$$1."sect".($$2+$$inc-1)%ge;' $$db > $$db-$$inc ;\
		echo "<!entity $$idname-$$inc SYSTEM '$$db-$$inc'>" >&3 ;\
		echo "&$$idname-$$inc;" >&4 ;\
	done ;\
	} 3>>$(DOCXX_GEN)/declare_entities.sgml 4>>$(DOCXX_GEN)/include_entities.sgml

###########################################################################
## Create example sections from testsuite code

.examples_made : 
	@if [ ! -f .examples_made ] ; then $(MAKE) -C $(TOP)/doc --no-print-directory examples ; fi

examples : FORCE
	@$(ECHO_N) 'Building examples:'
	@$(subst DIR,$(EXAMPLES_GEN),$(ENSURE_EMPTY_DIR))
	@$(ECHO_N) '' > $(EXAMPLES_GEN)/declare_entities.sgml
	@$(ECHO_N) '' > $(EXAMPLES_GEN)/include_entities.sgml
	@for e in $(EXAMPLE_TO_DOCUMENT) XXXX ;\
		do \
		if [ $$e = XXXX ] ; then break ; fi ;\
		$(ECHO_N) " $$e" ;\
		sect=$(EXAMPLES_GEN)/$${e}_example_section.sgml ;\
		ent=`echo "$${e}examplesection"| tr -dc '[a-z]'` ;\
		$(EST_HOME)/bin/cxx_to_docbook -s 1 $(TOP)/testsuite/$${e}_example.cc >$$sect 2>$(EXAMPLES_GEN)/$${e}_trace;\
		echo "<!entity $${ent} SYSTEM '$$sect' >" >> $(EXAMPLES_GEN)/declare_entities.sgml ;\
		echo "&$${ent};" >> $(EXAMPLES_GEN)/include_entities.sgml ;\
	done
	@echo " :done"
	@date > .examples_made

###########################################################################
## Create example sections from mainlines

.manuals_made : 
	@if [ ! -f .manuals_made ] ; then $(MAKE) -C $(TOP)/doc --no-print-directory manuals ; fi

manuals : FORCE
	@$(ECHO_N) 'Building manuals:'
	@$(subst DIR,$(MANUALS_GEN),$(ENSURE_EMPTY_DIR))
	@$(ECHO_N) '' > $(MANUALS_GEN)/declare_entities.sgml
	@$(ECHO_N) '' > $(MANUALS_GEN)/include_entities.sgml
	@for m in $(MAIN_TO_DOCUMENT) XXXX ;\
		do \
		if [ "$$m" = XXXX ] ; then break ; fi ;\
		$(ECHO_N) " $$m" ;\
		sect="$(MANUALS_GEN)/$${m}_manual_section.sgml" ;\
		ent=`echo "$${m}manualsection"| tr -dc '[a-z]'` ;\
		$(EST_HOME)/bin/cxx_to_docbook -s 1 \
			-special @options '<para>' "$(EST_HOME)/bin/$$m -sgml_options" '</para>' \
			-special @synopsis '<para>' "$(EST_HOME)/bin/$$m -sgml_synopsis" '</para>' \
			$(TOP)/main/$${m}_main.cc > $$sect 2>$(MANUALS_GEN)/$${m}_trace ;\
		echo "<!entity $${ent} SYSTEM '$$sect' >" >> $(MANUALS_GEN)/declare_entities.sgml ;\
		echo "&$${ent};" >> $(MANUALS_GEN)/include_entities.sgml ;\
	done
	@echo " :done"
	@date > .manuals_made

###########################################################################
## Index building

index.sgml: index_html.jade
	@echo 'Creating Index'
	$(EST_HOME)/bin/build_docbook_index -m html  -t 'INDEX' index_html.jade index.sgml

ensure_index_input:
	@if [ ! -f index_html.jade ] ; then echo "CREATING EMPTY INDEX" ; touch index_html.jade ; fi

###########################################################################
## Process things through LaTeX into gifs

tex_conversion : .tex_done
	@:

.tex_done: tex_stuff.jade
	@echo 'Converting LaTeX'
	@$(subst DIR,$(HTML_DIR)/$(IMAGES_GEN),$(ENSURE_EMPTY_DIR)) 
	@$(EST_HOME)/bin/tex_to_images -s $(TEX_SCALE) -d $(HTML_DIR) tex_stuff.jade
	@date >.tex_done

ensure_tex_input:
	@if [ ! -f tex_stuff.jade ] ; then echo "CREATING EMPTY LaTeX input" ; touch tex_stuff.jade ; fi

###########################################################################
## Include and exclude file building

INCLUDE_ONLY-full: $(DOCNAME).sgml
	@echo "Rebuild INCLUDE_ONLY-full"
	@$(RM) -f INCLUDE_ONLY-full INCLUDE_ONLY-min
	@{ \
	echo "<!--" ;\
	echo "  ..   Change INCLUDE to IGNORE to exclude a section" ;\
	echo "  -->" ;\
	sed -n -e '/.*%\(include-[a-zA-Z0-9]*\);.*/{;s//<!entity % \1 "INCLUDE">/;p;}' $(DOCNAME).sgml ;\
	} > INCLUDE_ONLY-full
	@sed -e '/INCLUDE/s//IGNORE/' INCLUDE_ONLY-full > INCLUDE_ONLY-min
	@chmod -w INCLUDE_ONLY-full INCLUDE_ONLY-min

sections.dtd: $(DOCNAME).sgml
	@echo "Rebuild sections.dtd"
	@$(RM) -f sections.dtd
	@{ \
	echo "<!--" ;\
	echo "  ..   Created automagically from $(DOCNAME).sgml" ;\
	echo "  -->" ;\
	sed -n -e '/.*%include-.*&\([a-z0-9]*\)doc;.*/{;s//<!entity \1doc SYSTEM "\1.sgml">/;p;}' $(DOCNAME).sgml ;\
	echo '<!entity indexChapter SYSTEM "index.sgml">' ;\
	} > sections.dtd
	@chmod -w sections.dtd

INCLUDE_ONLY: INCLUDE_ONLY-full
	@[ -f INCLUDE_ONLY ] || { cp INCLUDE_ONLY-full INCLUDE_ONLY ; echo INCLUDE_ONLY created from INCLUDE_ONLY-full ; }
	@chmod +w INCLUDE_ONLY

copy_requirements: FORCE
ifdef HTML_REQUIREMENTS
	@for r in $(HTML_REQUIREMENTS) ;\
		do \
		$(RM) -rf "$(HTML_DIR)/$$r";\
		cp -r "$$r" $(HTML_DIR) ;\
	done
endif



###########################################################################
## Make sure things get done in the right order

prepare_doc: doc++ manuals examples 
	@:

prepare_doc_quick: .doc++_made .manuals_made .examples_made 
	@:

.$(DOCNAME)_html : $(DOCNAME).sgml $(SGMLFILES) INCLUDE_ONLY $(DSSSL) .doc++_made .manuals_made .examples_made index.sgml tex_stuff.jade 

.$(DOCNAME)_ps : $(DOCNAME).sgml $(SGMLFILES) INCLUDE_ONLY $(DSSSL) .doc++_made .manuals_made .examples_made 

new_doc: INCLUDE_ONLY sections.dtd ensure_index_input ensure_tex_input prepare_doc new_doc_x copy_requirements

new_doc_x:
	@i=x; until $(MAKE) -q .$(DOCNAME)_html  ;\
		do \
		if [ "$$i" = xxxx ] ; then break ; fi ;\
		i="x$$i"; \
		$(MAKE) --no-print-directory .$(DOCNAME)_html  ;\
		$(MAKE) --no-print-directory tex_conversion  ;\
	done

new_quick_doc: INCLUDE_ONLY sections.dtd ensure_index_input ensure_tex_input prepare_doc_quick .$(DOCNAME)_html copy_requirements

quick_doc quick_new_doc: new_quick_doc
	@:

###########################################################################
## Actual Docbook=> output rules


.%_html: %.sgml
	sgmltools -b html -s $(DSSSL_HTML) $*.sgml
	date > .$*_html
	if cmp $(HTML_DIR)/HTML.index index_html.jade ; then : ; else cp $(HTML_DIR)/HTML.index index_html.jade ; fi
	if cmp $(HTML_DIR)/TeX.formulae tex_stuff.jade ; then : ; else cp $(HTML_DIR)/TeX.formulae tex_stuff.jade ; fi

.%_ps: %.sgml
	sgmltools -b ps $*.sgml
	@date > .$*_ps

.%_txt: %.sgml
	sgmltools -b txt -s $(DSSSL_HTML) $*.sgml


###########################################################################
## Some help with what the targets are.

new_doc_help: FORCE
	@echo '\
	;\
	;To do a complete rebuild of the documentation do: \
	;	gnumake new_doc ;\
	;\
	;To just remake the documentation without doing all the\
	;analysis of header files and so on, do:\
	;	gnumake quick_new_doc\
	;\
	;You can force rebuilding of just some of the supporting \
	;information using the following targets:\
	;\
	;	doc++file_list \
	;		 - Check for new files which doc++ needs to analyse. \
	;	doc++	 - analysis of headers and conversion to sgml. \
	;	manuals  - Extraction of manuals from executables. \
	;	examples - Extraction of example code. \
	;\
	;For instance, to rebuild the manual pages and then process: \
	;	gnumake manuals quick_new_doc \
	;\
	;\
	' | tr ';' '\012' 
