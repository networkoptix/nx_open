Classification and Regression Trees   {#estwagon}
===================================
[TOC]

# Overview {#cart-overview}

As part of tools for statistical modelling, EST includes methods
for automatically building decision trees and decision lists from 
features data to predict both fixed classed (classification) or
gaussians (regression).  \ref Wagon is
the basic program that provide this facility.

The construction of CARTs (classification and regression trees) is
best described in @cite breiman1984classification and has become a common basic
method for building statistical models from simple feature data.
CART is powerful because it can deal with incomplete data, multiple
types of features (floats, enumerated sets) both in input features and
predicted features, and the trees it produces often contain rules
which are humanly readable.

Decision trees contain a binary question (yes/no answer) about
some feature at each node in the tree.  The leaves of the tree
contain the best prediction based on the training data.   Decision
lists are a reduced form of this where one answer to each question
leads directly to a leaf node.  A tree's leaf node may be a single
member of some class, a probability density function (over some
discrete class), a predicted mean value for a continuous feature
or a gaussian (mean and standard deviation for a continuous value).

Theorectically the predicted value may be anything for which a function
can defined that can give a measure of *impurity*
for a set of samples, and a distance measure between impurities.


The basic algorithm is given a set of samples (a feature vector) find
the question about some feature which splits the data minimising the
mean "impurity" of the two partitions.  Recursively apply this
splitting on each partition until some stop criteria is reached
(e.g. a minimum number of samples in the partition.


The basic CART building algorithm is a *greedy algorithm
in that it chooses the locally best
discriminatory feature at each stage in the process.  This is
suboptimal but a full search for a fully optimized set of question
would be computationallly very expensive.  Although there are
pathological cases in most data sets this greediness is not a problem.

The basic building algorithm starts with a set of feature vectors
representing samples, at each stage all possible questions for all
possibles features are asked about the data finding out how the
question splits the data.  A measurement of impurity of each
partitioning is made and the question that generates the least impure
partitions is selected.  This process is applied recursively on each
sub-partions recursively until some stop criteria is met (e.g. a
minimal number of samples in a partition).

## Impurities {#estwagonimpurities}

The *impurity* of a set of samples is designed
capture how similar the samples are to each other.  The smaller
the number the less impure the sample set is.

For sample sets with continuous predictees Wagon uses the variance
times number of sample points.  The variance alone could
be used by this overly favour very small sample sets.  As the
test thatuses the impurity is trying to minimise it over
a partitioning of the data, multiple each part with the number
of samples will encourage larger partitions, which we have found
lead to better decision trees in general.

For sample sets with discrete predictees Wagon uses the entropy
times number of sample points.  Again the number of sample
points is used in so that small sample set are not unfairly favoured. 
The entropy for a sample set is calculated as:

\f[ E = \sum_{x \in class} prob(x) * \log(prob(x)) \f]

Other impurity measure could be used if required.  For example an
experimental cluster technique used for unit selection actually used
impurity calculated as the mean euclidean distance between all vectors
of parameters in the sample set.  However the above two are more
standard measures.

## Question forming {#estwagonoverviewquestion}

Wagon has to automatically form questions about each feature in the
data set.  

For discrete features questions are build for each member of the set,
e.g. if feature n has value x.  Our implementation does not currently
support more complex questions which could achieve better results
(though at the expense of training time).  Questions about features
being some subset of the class members may give smaller trees.  If the
data requires distinction of values a, b and c, from d e and f, our
method would require three separate questions, while if subset
questions could be formed this could be done in one step which would
not only give a smaller tree but also not unecessarily split the
samples for a, b and c.  In general subset forming is exponential on
the number items in the class though there are techniques that can
reduce this with heuristics.  However these are currently not supported.
Note however the the tree formalism produced but Wagon does support
such questions (with the operator "in") but Wagon will never produce
these question, though other tree building techniques (e.g. by hand)
may use this form of question.

For continuous features Wagon tries to find a partition of
the range of the values that best optimizes the average
impurity of the partitions.  This is currently done by linearly
splitting the range into a predefined subparts (10 by default)
and testing each split.  This again isn't optimal but does
offer reasonably accuracy without require vast amounts of
computation.


## Tree Building criteria {#estwagonoverviewtreebuild}

There are many ways to constrain the tree building algorithm to help
build the "best" tree.  Wagon supports many of theses (though there
are almost certainly others that is does not.

In the most basic forms of the tree building algorithm a fully
exhaustive classifcation of all samples would be achieved.  This, of
course is unlikely to be good when given samples that are not
contained within the training data.  Thus the object is to build a
classification/regression tree that will be most suitable for new
unseen samples.  The most basic method to achieve this is not to build
a full tree but require that there are at least n samples in a
partition before a question split is considered.  We refer to that as
the *stop* value.  A number like 50 as a stop value
will often be good, but depending of the amount of data you have, the
distribution of it, etc various stop value may produce more general
trees.

A second method for building "good" trees is to *hold out* some of 
the training data and build a (probably over-trained) tree with a small 
stop value.  Then prune the tree back to where it best matches the
held out data.  This can often produce better results than a fixed
stop value as this effectively allows the stop value to vary through
different parts of the tree depending on how general the prediction
is when compared against held out data.


It is often better to try to build more balanced trees.  A small stop
value may cause the tree building algorithm to find small coherent
sets of samples with very specific questions.  The result tree becomes
heavily lop-sided and (perhaps) not optimal.  Rather than having the
same literal stop value more balanced trees can built if the stop
value is defined to be some percentage of the number of samples under
consideration.  This percentage we call a *balance*
factor.  Thus the stop value is then the largest of the
defined fixed stop value or the balance factor times the number of
samples.


To some extent the multiplication of the entropy (or variance) by
the number of samples in the impurity measure is also way to
combat imbalance in tree building.


A good technique we have found is to build trees in a *stepwise*
fashion. In this case instead of considering all
features in building the best tree. We increment build trees looking
for which individual feature best increases the accuracy of the build
tree on the provided test data.  Unlike within the tree building
process where we are looking for the best question over all features
this technique limits which features are available for consideration.
It first builds a tree using each and only the features provided
looking for which individual feature provides the best tree.  The
selecting that feature is builds n-1 trees with the best feature from
the first round with each of the remaining features.  This process
continues until no more features add to the accuracy or some
stopping criteria (percentage improved) is not reached.


This technique is also a greedy technique but we've found that when
many features are presented, especially when some are highly
correlated with each other, stepwise building produces a significantly
more robust tree on external test data.  It also typically builds
smaller trees. But of course there is a cost in computation time.

While using the stepwise option each new feature added is printed out.
Care should be taking in interpreting what this means.  It does not
necessarily give the order and relative importance of the features,
but may be useful if showing which features are particualrly important
to this build.

Stepwise tests each success tree against the specified test set, (balance,
held out and stop options are respected for each build).  As this
is using the test set which optimizing the tree, it is not valid
to view the specified test set as a genuine test set.  Another
externally held test set should be used to test the accuracy of
generated tree.

## Data format {#cart-formats}

The input data for wagon (and some other model building tools
in the Edinburgh Speech Tools library), should consist of 
feature vectors, and a description of the fields in these vectors.

### Feature vectors {#estwagon-featvectors}

A feature vector is a file with one sample per line, with feature value
as white space separated tokens.  If your features values conatin
whitespace then you must quote them using double quotes.


The (Festival) program `dumpfeats` is specifically
designed to generate such files from databases of utterances but
these files may be generated from any data source.

Each vector must have the same number of features (and in the
same order.  Features may be specified as "ignored" in the
description (or in actual use) so it is common that data files
contain more features than are always used in model building.  By
default the first feature in a data file is the predictee, though
at least in wagon) the predictee field can be named at tree building time
to be other than the first field.

Features can be discrete of continuous but at present must be
single valued, "multi-valued" or "list-valued" features are not
currently supported.  Note this means that a feature in different
samples may have different values but in a particular sample a particular
feature can only have one value.

A type example is:

    0.399 pau sh  0   0     0 1 1 0 0 0 0 0 0 
    0.082 sh  iy  pau onset 0 1 0 0 1 1 0 0 1
    0.074 iy  hh  sh  coda  1 0 1 0 1 1 0 0 1
    0.048 hh  ae  iy  onset 0 1 0 1 1 1 0 1 1
    0.062 ae  d   hh  coda  1 0 0 1 1 1 0 1 1
    0.020 d   y   ae  coda  2 0 1 1 1 1 0 1 1
    0.082 y   ax  d   onset 0 1 0 1 1 1 1 1 1
    0.082 ax  r   y   coda  1 0 0 1 1 1 1 1 1
    0.036 r   d   ax  coda  2 0 1 1 1 1 1 1 1

Note is it common to have thousands, even hundreds of thousands of
samples in a data file, and the number of features can often be in the
hundreds, though can also be less than ten depending on the what it
describes.

### Data descriptions {#estwagon-datadescr}

A data file also requires a description file which names and classifies
the features in a datafiles.  Features must haves names so they can
be refered to in the decision tree (or other model output) and also
be classified into their type.  The basic types available for
features are:

  - `continuous` for features that range over reals (e.g. duration of phones)
  - `categorial` for features with a pre-defined list of possible values (e.g. phone names)
  - `string` for features with an open class of discrete values (e.g. words)
  - `vectors` like floats but as vectors of floats, (e.g. MFCC data)

The data description consists of a parenthesized list of feature
descriptions.  Each feature description consists of the feature
name and its type (and/or possible values).  Feature names, by
convention, should be features names in the sense for features (and
pathnames) used throughout the utterance structures in the
Edinburgh Speech Tools.  The expected method to use models
generated from features sets in the Edinburgh Speech Tools is
to apply them to items.  In that sense having a feature name be
a feature of an item (or relatve) pathname will avoid having
the extra step of extracting features into a separated table before
applying the model.  However it should also be stated that to
wagon these names are arbitrary tokens and their semantic irrelevant
at training time.

A typical description file would look like this, this is
one suitable for the data file given above



    ((segment_duration float)
     ( name  aa ae ah ao aw ax ay b ch d dh dx eh el em en er ey f g 
        hh ih iy jh k l m n nx ng ow oy p r s sh t th uh uw v w y z zh pau )
     ( n.name 0 aa ae ah ao aw ax ay b ch d dh dx eh el em en er ey f g 
        hh ih iy jh k l m n nx ng ow oy p r s sh t th uh uw v w y z zh pau )
     ( p.name 0 aa ae ah ao aw ax ay b ch d dh dx eh el em en er ey f g 
        hh ih iy jh k l m n nx ng ow oy p r s sh t th uh uw v w y z zh pau )
     (position_type 0 onset coda)
     (pos_in_syl float)
     (syl_initial 0 1)
     (syl_final   0 1)
     (R:Sylstructure.parent.R:Syllable.p.syl_break float)
     (R:Sylstructure.parent.syl_break float)
     (R:Sylstructure.parent.R:Syllable.n.syl_break float)
     (R:Sylstructure.parent.R:Syllable.p.stress 0 1)
     (R:Sylstructure.parent.stress 0 1)
     (R:Sylstructure.parent.R:Syllable.n.stress 0 1)
    )

There are also a number of special symbols that may be used in a
description file.  If the type (first toke after the name) is
`ignore` the feature will be ignored in the model
building process.  You may also specified features to ignore at tree
building time but it is often convenient to explicitly ignore
feature(s) in the description file.

For open categorial features the token `_other_`
should appear as the first in the list of possible values.  This
actually allows features to have a partially closed set and and open set.

A description file can't be generated automatically from a data set though
an approximation is possible.  Particularly its is not possible
to automatically decied if a feature value is continous of that its
example values happen to look like numbers.  The script
`make_wagon_desc` takes a datafile and file
containing only the names of the features, and the name of
the description file it will create.  This is often a useful
first pass though it almost certainly must be hand editted afterwards.

### Tree format {#estwagon-treeformat}

The generated tree files are written as Lisp s-expressions as this
is by far the easiest external method to represent trees.  Even if
the trees are read by something other than Lisp it is easy to
write a reader for such a format.  The syntax of a tree is


    TREE ::= LEAF | QUESTION-NODE
    
    QUESTION-NODE ::= "(" QUESTION YES-NODE NO-NODE ")"
    
    YES-NODE ::= TREE
    
    NO-NODE ::= TREE
    
    QUESTION ::= "(" FEATURENAME "is" VALUE ")" |
                 "(" FEATURENAME "=" FLOAT ")" |
                 "(" FEATURENAME "<" FLOAT ")" |
                 "(" FEATURENAME ">" FLOAT ")" |
                 "(" FEATURENAME "matches" REGEX ")" |
                 "(" FEATURENAME "in" "(" VALUE0 VALUE1 ... ")" ")"
    
    LEAF ::= "(" STDDEV MEAN ")" |
             "(" "(" VALUE0 PROB0 ")" "(" VALUE1 PROB1 ")" ... MOSTPROBVAL ")" |
             any other lisp s-expression    


Note that not all of the question types are generated by Wagon but 
they are supported by the interpreters.

The leaf nodes differ depending on the type of the predictee.  For
continuous predictees (regression trees) the leaves consist of a pair
of floats, the stddev and mean.  For discrete predictees
(classification trees) the leaves are a probability density function
for the members of the class.  Also the last member of the list is the
most probable value.  Note that in both case the last value of the
leaf list is the answer desired in many cases.


Here's a small example tree

       NOT WRITTEN


# Functions {#estwagon-functions}

# Programs {#estwagon-programs}

The following exectutable programs are provided for the
building and testing of decision trees

  - \ref wagon_manual: is the core building program
  - \ref wagon_test_manual: applies a trained treee to data and tests its accuracy.


