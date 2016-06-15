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

CENTER_COMMAND = $(AWK) '{n=(80-length($$0))/2; f=sprintf("%%%ds\n", n+length($$0)); printf(f, $$0);}'
UNDERLINE_COMMAND = sed -e '{p;s/./=/g;}'
INDENT_COMMAND = sed -e 's/^/		/'
KV_COMMAND = 	$(AWK) 'NF>1 {printf("%"  ind  "s%s%s\n", $$1, sep, $$2==""?"<UNSET>":$$2);next;}{print}'

real-info: 
	@{ \
	echo '---------------------------------------------------------------' ;\
	echo '' ;\
	echo '$(PROJECT_NAME) v$(PROJECT_VERSION) Configuration' | $(UNDERLINE_COMMAND) | $(CENTER_COMMAND);\
	echo '' ;\
	echo '$(PROJECT_PREFIX)_HOME:=:$($(PROJECT_PREFIX)_HOME)' ;\
	if [ '$(PROJECT_PREFIX)' != EST ] ;\
		then \
		echo 'EST:=:$(EST)' ;\
		echo 'EST_HOME:=:$(EST_HOME)' ;\
	fi ;\
	echo 'SYSTEM_TYPE:=:$(SYSTEM_TYPE)' ;\
	echo 'COMPILER:=:$(COMPILER_DESC) '`$(COMPILER_VERSION_COMMAND)` ;\
	if [ '$(JAVA_COMPILER)' != none ] ;\
		then \
		echo 'JAVA_COMPILER:=:$(JAVA_COMPILER_DESC) '`$(JAVA_COMPILER_VERSION_COMMAND)` ;\
		echo 'JAVA_HOME:=:$(JAVA_HOME)' ;\
		echo 'JMF_HOME:=:$(JMF_HOME)' ;\
		echo 'JSAPI_HOME:=:$(JSAPI_HOME)' ;\
	fi ;\
	echo 'CONFIGURATION:=:$(CONFIGURATION)' ;\
	if [ -n '$(LOCAL_REPOSITORY)' ] ;\
		then \
		echo '' ;\
		echo 'CENTRAL_DIR:=:$(CENTRAL_DIR)' ;\
	fi ;\
	echo '' ;\
	echo 'NATIVE_AUDIO_MODULE:=:$(NATIVE_AUDIO_MODULE)' ;\
	if [ -n '$(INCLUDE_MODULES)' ] ;\
		then \
		echo 'INCLUDE_MODULES:=:    ' ;\
		{ : ; $(foreach I,$(sort $(INCLUDE_MODULES)),echo '$(I):$(subst ',,$(MOD_DESC_$(I)))';) }| $(KV_COMMAND) FS=: sep=' ' ind=28 ;\
	fi ;\
	echo '' ;\
	echo 'INCLUDES:=:$(INCLUDES)' ;\
	echo 'TEMPLATES:=:$(TEMPLATES)' ;\
	echo 'DEFINES:=:$(DEFINES)' ;\
	echo 'LIBS:=:$(LIBS)' ;\
	if [ '$(JAVA_COMPILER)' != none ] ;\
		then \
		echo 'JAVA_CLASSPATH:=:$(JAVA_CLASSPATH)' ;\
	fi ;\
	echo '' ;\
	echo '---------------------------------------------------------------' ;\
	} | $(KV_COMMAND) FS=':=:' sep=' = ' ind=22


ifndef REAL_MODULES
modules:
	@$(MAKE) --no-print-dir REAL_MODULES=1 INCLUDE_EVERYTHING=ALL modules
else
modules: 
	@{ \
	echo '---------------------------------------------------------------' ;\
	echo '' ;\
	echo 'Available $(PROJECT_NAME) Modules'|$(UNDERLINE_COMMAND)|$(CENTER_COMMAND) ;\
	echo '' ;\
	{ $(foreach I,$(sort $(ALL_MODULES)),echo '$(I):$(subst ',,$(MOD_DESC_$(I)))';) } |$(KV_COMMAND) FS=: sep='   ' ind=20 ;\
	echo '' ;\
	echo '---------------------------------------------------------------' ;\
	}

endif

.config_error::
	@: nothing
