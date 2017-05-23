Manuals {#estmanuals}
=========================

# General Information {#estmanualsgeneral}

Edinburgh Speech Tools provides a set of executables, which
offer access to speech tools functionality in the form of a stand
alone program. As far as possible, the  programs follow a common
format in terms of assumptions, defaults and processing paradigms.

The following are generally true of most speech tools programs.

  - Arguments to functions can be specified in any order on the command line. Most programs can take multiple input files, which by default have no preceding argument.
  - Output by default is to standard out. The -o flag can be used to specify an output file.
  - Often programs can read many formats of input file.
    Wherever possible, this is done automatically. If this can't be done, the
    -itype flag can be used to specify the input file format.
  - The output file format is specified by -otype.

Specific documentation for each executable is given on separate pages:

  - @subpage ch_wave_manual
  - @subpage ch_track_manual

  - @subpage tilt_analysis_manual
  - @subpage tilt_synthesis_manual

  - @subpage sigfv_manual
  - @subpage spectgen_manual
  - @subpage sigfilter_manual
  - @subpage designfilter_manual
  - @subpage pda_manual
  - @subpage pitchmark_manual

  - @subpage dp_manual
  - @subpage ngram_build_manual
  - @subpage ngram_test_manual
  - @subpage viterbi_manual

  - @subpage na_play_manual
  - @subpage na_record_manual

  - @subpage wagon_manual
  - @subpage ols_manual
  - @subpage ols_test_manual

  - @subpage wfst_build_manual
  - @subpage wfst_train_manual
  - @subpage wfst_test_manual

  - @subpage scfg_make_manual
  - @subpage scfg_train_manual
  - @subpage scfg_parse_manual
  - @subpage scfg_test_manual

  - @subpage siod_manual
  - @subpage fringe_client_manual
  - @subpage bcat_manual
  - @subpage xml_parser_manual

