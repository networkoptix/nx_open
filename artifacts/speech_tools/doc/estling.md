Linguistic Classes {#estling}
=======================

[TOC]

# Introduction {#eslingintro}

EST offers a comprehensive integrated system for handling and storing linguistic information of all types. This system is based on the *Heterogeneous Relation Graph formalism*. There is a basic hierarchy of 5 main classes in this part of the library:

  - **Val**: The EST_Val class holds single atomic entities, such as numbers or strings. A EST_Val can be thought of as a single variable which can hold a value of a variety of types. It can hold one of 3 built in types (`int`, `float`, EST_String), plus the ability to have an "other" type which is user-defined. In that way, a *val* can represent a list, waveform, track or other any other type. 

  - **Features**: The EST_Features class is a key/value list of many EST_Val. Because of the type flexibility of EST_Val, EST_Features can be the type of a EST_Val, which allows nested feature structures. 

  - **Items**: The EST_Item class represents basic linguistic entities such as words, phonemes, syllables, phrases etc. It is basically a wrap-around for EST_Features 

  - **Relations**: A relation is used to represent the structural relationship between groups of items. A single relation would hold all the phones for an utterance or all the words for an utterance. Relations can take a number of different structural types: it is common to use a list for storing words and phones, a tree for syntactic and prosodic structure and a multi-linear structure for non-hierarchical linguistic information such as an autosegmental tone diagram.

  - **Utterance**: A Utterance structure contains all the relations for an utterance.

## Relations {#estlingrelations}

Relations store ordered collections of linguistic items. The most common basic types are the list and the tree. \ref figure-6-1 "Figure 6-1" shows two examples of relations. \ref figure-6-1 "Figure 6-1a" shows a word relation of the sentence "this is an example". This relation is a simple linear list. \ref figure-6-1 "Figure 6-1b" shows a syntax tree of the same sentence. Both figures are comprised of three components: items, nodes and arcs. The items contain the linguistic information (in this case the name of the word or syntactic category), whereas the nodes and arcs define the relationship between the items.

\anchor figure-6-1
\image html relations.svg "Figure 6-1: Relations"
\image latex relations.eps "Relations" width=7cm


In the list case, the arcs occur in complimentary pairs named next and previous, allowing forward and backwards traversal. In the tree case the arcs occur in complimentary pairs named `parent` and `daughter1`, `daughter2` etc. All arcs in a HRG occur in named complimentary pairs: by convention `next` and `previous` are used for lists, and `parent`, `daughter` etc for trees.

Formally, relations can be seen as either directed graphs, where arcs occur in complimentary named pairs, or as acyclic graphs, in which an arc has two complimentary names. A node may have any number of arcs. The names of the arcs in the outgoing direction must all be unique, whereas the names of the arcs in the incoming direction do not have to be unique. (For example in the tree, a node can have two incoming nodes named `parent` for traversal from its daughters, but the names of the nodes to those daughters must be called something like `left daughter` and `right daughter`.)

In these simple examples, the use of nodes and items is clearly redundant as all nodes and items have a one-to-one relationship. However, this is often not the case in that although a node can be linked to only one item, an item can be linked to many nodes, so long as each is in a different relation. \ref estling-figure-6-2 "Figure 6-2" shows a structure representing the combination of \ref figure-6-1 "Figure 6-1a" and \ref figure-6-1 "Figure 6-1b". The syntax and word relations are the same, but the nodes of the word list and the terminal nodes of the syntax tree point to the same items. This a the key feature of the HRG system. Of course it would be possible to keep the information separate as in \ref figure-6-1 "figure 6-1", but this is redundant because these items really do represent the same word. As will be seen below items can be very complex structures and it would be wasteful to duplicate the information. More importantly, information in items is often changed in the synthesis process and there is a chance of the syntax and word items becoming inconsistent. A list of named backwards links to nodes is kept in each item, so that given an item, any node in any relation linked to that item can be easily found.

\anchor estling-figure-6-2
\image html relations_2.svg "Figure 6-2: Multiple relation types"
\image latex relations_2.eps "Multiple relation types" width=7cm

In a simple examples such as this, it is possible to imagine a situation where the word relation is defined to be a traversal of the terminal nodes in the syntax relation. However, in the general case, more complex intersecting relations are often required and such simple approaches will fail.

## Items and Features {#estlingitemsandfeatures}

Items are attribute value lists (AVL) which contain linguistic information. All "atomic" linguistic entities such as words, syllables and phones are represented by items.

In the simple sense, attributes are named linguistic properties such as part-of-speech, place of articulation, duration etc, and values are strings, enumerated sets (that is, a value from a fixed list of possibilities), floating point numbers and integers. For instance, this is a typical word item:

\f[
\left [ 
\begin{array}{ll}
\mbox{POS} & \mbox{\emph{Noun}} \\
\mbox{NAME} & \mbox{\emph{example}} \\
\mbox{FOCUS} & \mbox{+} \\ 
\end{array}  \right ]
\f]

and this is a typical phoneme item:

\f[
\left [ 
\begin{array}{ll}
\mbox{NAME} & \mbox{\emph{sh}} \\
\mbox{PLACE OF ARTICULATION} & \mbox{\emph{palatal}} \\
\mbox{MANNER} & \mbox{\emph{fricative}} \\ 
\mbox{VOICE} & \mbox{--} \\ 
\mbox{DURATION} & 0.234 \\ 
\end{array}  \right ]
\f]


Items are not directly typed, in that there is nothing in the AVL itself to say if it is a phoneme, syllable or word. Rather, typing comes from the fact that it is linked to a node in a relation. Items in the word relation are words, items in the syntax relation are syntactic constituents and items in both have both types.

Values need not be atomic types. Often it is unattractive to store all the information in an item as a single flat attribute-value list. For example, in a traditional binary phonological feature system such as SPE \cite spe descriptions of segments are typically given as:

\f[
\left [ 
\begin{array}{ll}
\mbox{NAME} & \mbox{\emph{d}} \\
\mbox{CORONAL} & + \\
\mbox{ANTERIOR} & + \\ 
\mbox{VOICE} & + \\ 
\mbox{CONTINUANT} & \mbox{--} \\
\mbox{SONORANT} & \mbox{--} \\ 
\end{array}
\right ]
\f]


However, it can be useful to provide a named hierarchy on the features:

\f[
\left [ 
\begin{array}{ll}
\mbox{NAME} & \mbox{\emph{d}} \\
\mbox{PLACE OF ARTICULATION \boxed{1} } & 
     \left [ \begin{array}{ll} 
                  \mbox{CORONAL} & \mbox{\emph{+}} \\
                  \mbox{ANTERIOR} & \mbox{\emph{+}} \\
             \end{array} \right ] \\
\mbox{VOICE} & \mbox{\emph{+}} \\ 
\mbox{CONTINUANT} & \mbox{\emph{--}} \\
\mbox{SONORANT} & \mbox{\emph{--}} \\ 
\end{array}  \right ]
\f]


This is useful in specifying the consequences of traditional phonological rules such as syllable-final nasal assimilation. In some situations, the place of articulation of a nasal is assimilated to the place of articulation of the following stop (e.g. "ten" + "bags" -> "tembags"). If the feature bundle mechanism is used, an additional mechanism (indicated by the reference \f$\boxed{1}\f$) allows the value of an item to be a AVL in a different item. In the above example the fact that the place of articulation is the same for the nasal and stop is shown by using a reference for the place of articulation of the nasal, which refers to the place of articulation of the stop. The use of the feature bundle to represent place of articulation allows this operation to be done with a single reference rather than by a two separate references to CORONAL and ANTERIOR.

\f[
\left [ 
\begin{array}{ll}
\mbox{NAME} & \mbox{\emph{d}} \\
\mbox{PLACE OF ARTICULATION } & \mbox{\boxed{1}}\\
\mbox{VOICE} & \mbox{\emph{+}} \\ 
\mbox{CONTINUANT} & \mbox{\emph{--}} \\
\mbox{SONORANT} & \mbox{\emph{--}} \\ 
\end{array}  \right ]
\f]

Often feature bundles are simply used as a tidying mechanism. For instance, it is usually not necessary to access phonological feature and timing information at the same time, so a neater mechanism for storing this on a phone would be:


\f[
\left [ 
\begin{array}{ll}
\mbox{NAME} & \mbox{\emph{d}} \\
\mbox{PHONOLOGICAL FEATURES} & \left [ 
        \begin{array}{ll}
             \mbox{PLACE OF ARTICULATION } & \left [ 
                  \begin{array}{ll} 
                      \mbox{CORONAL} & \mbox{\emph{+}} \\
                      \mbox{ANTERIOR} & \mbox{\emph{+}} \\
                   \end{array} \right ] \\
             \mbox{VOICE} & \mbox{\emph{+}} \\ 
             \mbox{CONTINUANT} & \mbox{\emph{--}} \\
             \mbox{SONORANT} & \mbox{\emph{--}} \\ 
        \end{array}  \right ] \\
\mbox{TIMING} & \left [ 
        \begin{array}{ll}
             \mbox{START} & 0.412 \\
             \mbox{END} & 0.489 \\ 
             \mbox{DURATION} & 0.077 \\
        \end{array}  \right ] \\
\end{array}  \right ]
\f]

The last important feature mechanism is function values, which is when a function rather than an atomic value is the value of an attribute. Discussion of function values is given in the section on timing as they are particularly relevant to that issue.

# Classes {#estlingclassessection}

\ref estlingclasses

# Functions {#estlingfunctions}

This is a sub-library of functions for creating, traversing and accessing relations which are trees:

## Tree traversal functions

\ref treetraversalfunctions

## Tree building functions

\ref treebuildfunctions


\subpage ling-example
