#!/bin/sh

# The .body source files contain documentation for executables.
# This files have the synopsis and the options section empty.
# The synopsis and the options documentation are generated automatically
# using the EST native sgml generated documentation options.
# The xsl transformations convert the sgml output from the EST executables
# into markdown format, parseable by doxygen.
#
# Usage: 
# ./create_dox.sh "../../bin/ch_wave" ./ch_wave_man.dox.body
# A ch_wave_manual.dox will be generated in current directory
full_program="$1"
doc_body="$2"
program=`basename $full_program`
doc_out="${program}_man.dox"

# Generate
$full_program -sgml_synopsis | xsltproc convert-synopsis.xslt - > "${program}_synopsis.txt"
$full_program -sgml_options | xsltproc convert-options.xslt - > "${program}_options.txt"

awk 'BEGIN {
        while ((getline line < ARGV[1]) > 0) {file1 = file1 nl line; nl = "\n"}; 
        close (ARGV[1]); nl = "";
        while ((getline line < ARGV[2]) > 0) {file2 = file2 nl line; nl = "\n"};
        close (ARGV[2]);
        ARGV[1] = ""; ARGV[2] = "" }
      { gsub("@SYNOPSIS@", file1); 
        gsub("@OPTIONS@", file2); 
        print }' "${program}_synopsis.txt" "${program}_options.txt" "${doc_body}" > "${doc_out}"

rm "${program}_synopsis.txt" "${program}_options.txt"
