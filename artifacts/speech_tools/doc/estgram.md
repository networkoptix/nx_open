Grammar  {#estgram}
=====================

# Overview {#estgramoverview}

To aid speech recognition and general text processing the speech
tools library provides an number of techniques to process various
types of grammars.

These consist of a continuing expanding set of class and related 
programs.  These are at various levels of development but
all have fairly stable and working basic features.

## N-grams {#estgramngrams}

Ngram language models are supported by the EST_Ngrammar class, and
associated routines and programs.   Support is provided for
building. tuning, smoothing and running n-gram models.  N-grams
themselves have been can be internally stored in either a 
*dense* or *sparse*.  In
the dense case all possible states are represented with probability
distributions, while the sparece case only has those possible
states for with data is available. Sparse will be much smaller
in very large cases.  However we consider the dense case to be
the most fully developed.

Formally n-grams can be views as a special case of weighted finite
state machines.  Our implementation reflects that where possible (backoff
options break this simple view), thus a start state is provided
and traversal operation are given.  This method of using n-grams is
by far the most efficient as only one new piece of information
is required at each stage, so no vectors of tokens need be collected
(or shifted) and presented to n-gram class.  However as this finite
state machine view can't always be reasonable used we also support
access through a vector of tokens.

### Building ngram language models {#estgramngrambuild}

The program \ref ngram_build_manual estimates ngram language 
models from data. The data can be in a number of formats and be 
saved in both an ascii (easier for humans to read) and binary
(quick to load) format.

#### Vocabularies

The vocabulary of an ngram must be predefined.  This is required to
allow efficient internal representation.  This implementation
supports two vocabularies, one for the n-1 tokens in an ngram
and one for the nth token as potentially this "predictee" could
be from a different class.  We also support the notion of
out of vocabulary word, so any token found in the input data
that is not in the vocabulary may be mapped to that token.

In build n-grams there are options on what to do with n-grams
which contain out of vocabulary tokens.  They may be mapped
to te specifed out of vocabulary word, the ngram can be ignored or
the whole sentence containing the out of vocabulary word can
be ignored.

#### ngram data input formats

The input data can be in a number of formats depending on how
much preprocessing you wish to do before building.   The most
basic form is to submit n-grams.  That is n tokens, on each
line.  For example for a tri-gram model of phones it might
look like

    0 # a1 
    # a1 f 
    a1 f r 
    f r i 
    r i k 
    i k aa1 
    k aa1 n 
    aa1 n @ 

In this case the data preparation stage most create each n-gram with
the sigle stepping through the data at each stage.  This
format we call `ngram_per_line`

A second format is `sentence_per_line` where each
line of a file is a complete "sentence".  Ngrams for each n-tuple 
will be automatically created and cumulated.   In this case the input
file might look like

    a1 f r i k aa1 n @
    ei1 b r @ h a m

In this mode, ngrams for the tokens at start of the sentence are
created by using the token by defining a `prev_tag` (and if necessary a
`prev_prev_tag`). Thus given the above sentence by line file,
a `prev_tag` of "#" and a `prev_prev_tag` of "0".  The first few
tri-grams cumulated are

    0 # a1
    # a1 f
    a1 f r

If the ngram size requires looking back further the `prev_prev_tag` is
repeat indefinitely.  Likewise an end_tag is appended to the end
of every sentence too, (i.e. end of every line).

A third data input format is `sentence_per_file`
where line breaks are no longer signficant and n-grams are create 
for all n-tuples in the file.  The same special cases are treated
for beginning and end of file as are for beginning and end of
line in the sentence_per_line case.

#### Smoothing and Backoff

We support a number of different techniques to deal with lack of data in
a training set. 

Good Turing smoothing \cite churchgale1991 is supported
allowing smoothing on n-grams whose frequency is less than M.  We also
support *backoff* where the n-1 grams are
(recursively) built to provide an estimation of probability
distributions for unseen n-grams.

### Testing ngram language models {#estngramtesting}

\ref ngram_test_manual computes language model entropy/perplexity 
on test data. The test data may be in any of the formats described
above.

## SCFGs {#estngramscfg}

Stochastic context-free grammars are a version of context-free grammars
with probabilities on rules.  In this implementation we assume SCFGs
are always in Chomsky Normal From (CNF) where rules may only be binary
or unary branching.  When binary, both daughters must be non-terminals and
when unary, the daughter must be a terminal.

The implementation here is primarily based on \cite pereira1992inside
thus allowing unsupervised training of SCFGs as well
as allowing seeding with a bracketed corpus which can vastly reduce
training time, and improve results.  Training uses the inside-outside
algorithm.

The support is split into four parts: making grammars, training grammars,
testing grammars and parsing.

A grammar file consists of a set of rules.  Each rule is a bracketed
list of probability, nonterminal, followed by two nonterminals or
one terminal.  A simple example is

    (0.5 S A D)
    (0.5 S C B)
    (0.79 B S C)
    (0.21 B a)
    (0.79 D S A)
    (0.21 D b)
    (1 A b)
    (1 C a)

The mother non-terminal in the first rule is the distinguished symbol.

Grammars may be constructed by hand, by the program \ref scfg_make_manual or
by some other external process.  The program \ref scfg_make_manual constructs
a full grammar given a list (or number of) terminals and nonterminals.
The rules can be assigned equal probabilities or random ones.  The
"probabilities" may be actual probabilities or log probabilties.
For example given a file `wp19` with a list of terminals, a grammar
suitable for training with 15 non-terminals may be created by the command

    scfg_make -nonterms 15 -terms wp19 -domain prob \
              -values random -o wsj_15_19.gram

The non-terminals or terminal names will be automatically generated if a
number is given, or will be as specified if a file name is given.  In
the case of a filename being given, no brackets should be the file just
whitespace separated tokens.

A corpus consists of a number of sentences, each sentence must be
contain within a set of parenthesis.  The sentences themselves may
additionally contain further bracketing (for training and testing).
Each sentence is read by the Lisp reader so comments (semi-colon to
end of file) may be included.  For example

\code{.lisp}
((((n n) punc ((cd n) j) punc)
 (md (v (dt n) (in (dt j n)) (n cd)))
 punc))
(((n n) (v ((n) (in ((n n) punc (dt n v n))))) punc))
((((n n) punc (((cd n) j) cc ((j n) (in (n n n n)))) punc)
 (v (v (((dt j n) (in (dt j j n))))))
 punc))
...
\endcode

Training is done by estimating the inside and outside probabilities of
the rules based on their current probabilities and a corpus.  The corpus
may optionally include internal bracketing which is used by the training
algorithm to precluded some possible parses hence making the training
typically faster (and sometimes more accurate).  After each training
pass the grammar rule probabilities are updated and the process starts
again.  Note depending on the number of training sentences training
passes may take a very long time.  After each passes the cross entropy
for the current version of the grammar is printed.  This should normally
decrease until the the "best" grammar has been found.

The program \ref scfg_train_manual takes an initial grammar, and corpus and,
by default will train for 100 passes.  Because it can take prohibitively
long for a desired number of passes an option is available to selection
only an N percent chunk of the training set on each pass, cycling
through the other N percent chunks of the corpus on each pass
Experiments have shown that this not only produces much faster training,
but the accuracy of the fully trained grammar is very similar.  Given the
choice of waiting taking 10 days or 48 hours to parse, it is highly
recommended.

After each N passes the current state of the grammar may be saved, the
number of passes between saving is specified by the `-checkpoint`
option.  The grammar is saved in the output file appended with the pass
number.

Because the partitioned training will select different partitions
depending on the pass number you can optionally specify the starting
pass number, making it much easier to continue training after some
interruption.

Testing is done by the program \ref scfg_test_manual it takes a grammar and a
corpus.  That corpus may be fully bracketed or not.  By default
the mean cross entropy value from anaylzing these senetences will
be printed, also the number sentence sthat fail to parse.

Alternatively a *bracketing accuracy* may be calculated this is the
percentage of prhases in a parsed sentence that are compatible with the
bracketing in the corpus example.

The fourth program provides a mechanism for parsing one or more
sentences.  The corpus this time should contain no bracketing except
around the beginning and end of the sentence itself.  Two forms of parses
are produced.  A full form with start and end points for each phrase,
the related non-terminal and the probability, and a simple form where
only the bracketing is given.  Note only one (or no) parses is given.
For any phrase only the best example tree is given though the
probability is given as the sum of all possibily derivations of that
non-terminal for that phrase.

    scfg_parse -grammar wsj_15_19.gram -corpus news.data -brackets -o news.out

Note the input for must be strings of terminals for the given grammar.
For real parsing of real text it is likely the grmmar uses part of
speech tags as terminals and the data is avtuall words not part of
speech tags.  If you want to parse texts then you can use the Festival
script `festival/examples/scfg_parse_text` which takes in arbitrary
text, runs the part of speech tagger on it after standard tokenizing
rules and parses the output saving the parse to the specified file.

## WFSTs {#estngramwfst}

The speech tools contains a small, but growing library of
basic functions for building, and manipulating weighted finite
state transducers.  Although not complete they already provided many of
the basic operations and compilers one needs for using these devices.

Given a WFST the following operations are supported: \ref deterimise,
\ref minimize, \ref complement.  

Given two WFSTs the following operations are supported: \ref intersection,
\ref union, \ref difference,  \ref concatenation and \ref compose.

In addition to these operations compiles are provided for a number of
basic input formats: regular expressions, regular grammars, context-free
grammars (with depth restriction) and Kay/Kaplan/Koksenniemi two-level
morphology rules.

Still missing are complete treatment of the weights through some
basic operations (e.g. minimization doesn't presever weights).  Also
techniques for learning WFSTs from data, or at least weightign existing
FSTs from data will be added in later versions.

In general inputing symbols is of the form X or X/Y.  When X is
given it is (except if using the wfst as a transducer) treated
as X/X.  Where X/Y is input/output symbols, thus using single symbols
will mostly cause the wfst mechanisms to act as if they are finite
state machines.

The two main programs are \ref wfst_build_manual and \ref wfst_run_manual.
\ref wfst_run_manual runs in both recognition and transduction mode.

\ref wfst_build_ builds wfst's from description files or through
combination of existing ones.  The output may be optionally
determinized or determinized and minimized.

### Kay/Kaplan/Koskenniemi morphological rules {#estngramwfstkaykaplan}

One of the major drives in interest in wfst has been through their
use in morphology @cite kaplan94.  Hence we provide a method for
compiling Kay/Kaplan/Koskenniemi type (restricted) context sensitive
rewrite rules.  The exact form is given in the example below. 

This example covers basic letters to letters but also Epenthesis for
e-insertion in words like `churches` and `boxes`.
\code{.lisp}
(KKrules
 engmorph
 (Alphabets
  ;; Input Alphabet
  (a b c d e f g h i j k l m n o p q r s t u v w x y z #) 
  ;; Output Alphabet
  (a b c d e f g h i j k l m n o p q r s t u v w x y z + #) 
 )
 (Sets
  (LET a b c d e f g h i j k l m n o p q r s t u v w x y z)
 )
 (Rules
 ;; The basic rules
 ( a => nil --- nil) 
 ( b => nil --- nil) 
 ( c => nil --- nil) 
 ( d => nil --- nil) 
 ( e => nil --- nil) 
 ( f => nil --- nil) 
 ( g => nil --- nil) 
 ( h => nil --- nil) 
 ( i => nil --- nil) 
 ( j => nil --- nil) 
 ( k => nil --- nil) 
 ( l => nil --- nil) 
 ( m => nil --- nil) 
 ( n => nil --- nil) 
 ( o => nil --- nil) 
 ( p => nil --- nil) 
 ( q => nil --- nil) 
 ( r => nil --- nil) 
 ( s => nil --- nil) 
 ( t => nil --- nil) 
 ( u => nil --- nil) 
 ( v => nil --- nil) 
 ( w => nil --- nil) 
 ( x => nil --- nil) 
 ( y => nil --- nil) 
 ( z => nil --- nil) 
 ( # => nil --- nil)
 ( _epsilon_/+ => (or LET _epsilon_/e) --- nil)

 ;; Epenthesis
 ;;   churches -> church+s
 ;;   boxes -> box+s
 (e/+ <=> (or (s h) (or s x z) (i/y) (c h))
	    ---
	    (s))
)
\endcode

A substantially larger example of morphographenic rules is distributed with
the Festival speech synthesis system in `festival/lib/engmorph.scm`.
This is based on the English description in @cite ritchie92 .

For a definition of the semantics fo the basic types of rule,
surface coercion, context restriction and combined rules
see @cite ritchie92.  Note that these rules are run in
parallel (the transducers are intersected) making they rule
interact in ways that the author might not intend.  A good rule
debugger is really required in order to write a substantial set
of rules in this formalism.

The rule compilation method used differs from Kay and Kaplan, and
also from @cite mohri96 and actually follows them method used
in @cite ritchie92 though in this case, unlike @cite ritchie92,
the technique is followed through to true wfst's.  The actual
compilation method shold be described somewhere.

The above may be compiled into a wfst by the command (assuming it is
in the file `mm.rules`.

    wfst_build -type kk -o engmorph.wfst -detmin engmorph.scm

This rule compiler has also been used in finding equivalent transducers
for restricted forms of decision tree (following @cite sproat96) and
may be view as mostly stable.

### Regular expressions {#estngramwfstregex}

A simple method for building wfst's from regular expressions is also
provided.

An example is
\code{.lisp} ((a b c)
 (a b c)
 (and a (+ (or b c)) d))
\endcode

This consists of the input alphabet and the output alphabet
followed by a LISP s-expression contains the regex.  The supported
operators are `and`, `or`, `+`, `*` and `not`.

Compilation is by the following command:

    wfst_build -type regex -o t1.wfst -detmin t1.regex

### Regular Grammars {#estngramwfstregulargrammars}

A compilation method also exists for regular grammars.  These grammars
do not need to be a normal form, in fact no chaeck is made that they
are regular, if they contain center-embedding the construct algorithm
will go into a loop and eventually run out of storage.  The
correction to that is to add a depth limit which would then
allow wfst approximations of context-free grammars, which would
be useful.

An example regular grammar is
\code{.lisp} (RegularGrammar
 engsuffixmorphosyntax
 ;; Sets 
 (
 (V a e i o u y)
 (C b c d f g h j k l m n p q r s t v w x y z)
 )
 ;; Rules

 (
 ;; A word *must* have a suffix to be recognized
 (Word -> # Syls Suffix )
 (Word -> # Syls End )

 ;; This matches any string of characters that contains at least one vowel
 (Syls -> Syl Syls )
 (Syls -> Syl )
 (Syl -> Cs V Cs )
 (Cs -> C Cs )
 (Cs -> )

 (Suffix -> VerbSuffix )
 (Suffix -> NounSuffix )
 (Suffix -> AdjSuffix )
 (VerbSuffix -> VerbFinal End )
 (VerbSuffix -> VerbtoNoun NounSuffix )
 (VerbSuffix -> VerbtoNoun End )
 (VerbSuffix -> VerbtoAdj  AdjSuffix )
 (VerbSuffix -> VerbtoAdj End )
 (NounSuffix -> NounFinal End )
 (NounSuffix -> NountoNoun NounSuffix )
 (NounSuffix -> NountoNoun End )
 (NounSuffix -> NountoAdj AdjSuffix )
 (NounSuffix -> NountoAdj End )
 (NounSuffix -> NountoVerb VerbSuffix )
 (NounSuffix -> NountoVerb End )
 (AdjSuffix -> AdjFinal End )
 (AdjSuffix -> AdjtoAdj AdjSuffix)
 (AdjSuffix -> AdjtoAdj End)
 (AdjSuffix -> AdjtoAdv End)  ;; isn't any Adv to anything

 (End -> # )  ;; word boundary symbol *always* present

 (VerbFinal -> + e d)
 (VerbFinal -> + i n g)
 (VerbFinal -> + s)

 (VerbtoNoun -> + e r)
 (VerbtoNoun -> + e s s)
 (VerbtoNoun -> + a t i o n)
 (VerbtoNoun -> + i n g)
 (VerbtoNoun -> + m e n t)

 (VerbtoAdj -> + a b l e)

 (NounFinal -> + s)

 (NountoNoun -> + i s m)
 (NountoNoun -> + i s t)
 (NountoNoun -> + s h i p)

 (NountoAdj -> + l i k e)
 (NountoAdj -> + l e s s)
 (NountoAdj -> + i s h)
 (NountoAdj -> + o u s)

 (NountoVerb -> + i f y)
 (NountoVerb -> + i s e)
 (NountoVerb -> + i z e)

 (AdjFinal -> + e r)
 (AdjFinal -> + e s t)

 (AdjtoAdj -> + i s h)
 (AdjtoAdv -> + l y)
 (AdjtoNoun -> + n e s s)
 (AdjtoVerb -> + i s e)
 (AdjtoVerb -> + i z e)

)
)
\endcode

The above is a simple morpho-syntax for English.


# Programs {#estngramprograms}

The following are exectutable programs for grammars

  - scfg_make_manual:  make a set of rules for a SCFG.
  - scfg_train_manual:  train a set of rules for a SCFG.
  - scfg_parse_manual: Perform parsing using pre-trained grammar
  - scfg_test_manual: Test the output of a parser.
  - wfst_build_manual: Build a weighted finite state transducer.
  - wfst_run_manual: Run a weighted finite state transducer.
  - ngram_build_manual: Train a n-gram from text.
  - ngram_test_manual: Calculate the perplexity etc of an n-gram.

# Classes {#estngramclasses}

  - EST_SCFG
  - EST_SCFG_Rule
  - EST_SCFG_traintest
  - EST_bracketed_string
  - EST_Ngrammar
  - EST_WFST
