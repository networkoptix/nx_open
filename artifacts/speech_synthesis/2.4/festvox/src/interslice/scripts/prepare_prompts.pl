#!/usr/local/bin/perl
use strict;
###########################################################################
##                                                                       ##
##                                                                       ##
##              Carnegie Mellon University, Pittsburgh, PA               ##
##                      Copyright (c) 2004-2005                          ##
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
##  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         ##
##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
##  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      ##
##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
##  THIS SOFTWARE.                                                       ##
##                                                                       ##
###########################################################################
##                                                                       ##
##          Author :  S P Kishore (skishore@cs.cmu.edu)                  ##
##          Date   :  June 2005                                          ##
##                                                                       ##
###########################################################################

my $nargs = $#ARGV + 1;
if ($nargs != 2) {
  print "Usage: perl file.pl <file1> <prompt-file>\n";
  exit;
}

my $inF = $ARGV[0];
my $prmF = $ARGV[1];

my @ln = &Get_ProcessedLines($inF);
my @para;
my $p = 0;

for (my $i = 0; $i <= $#ln; $i++) {
  my @wrd = &Get_Words($ln[$i]);
  my $nw = $#wrd + 1;
  if ($nw == 0) { 
    #print "$para[$p] \n ************\n";
    $p = $p + 1; 
  }else {
    $para[$p] = $para[$p]." ".$ln[$i];
  }
}

my $min = 1.0e+35;
my $max = -1.0e+35;
my $avg = 0;

open(fp_prm, ">$prmF");

for (my $j = 0; $j <= $#para; $j++) {
  my @wrd = &Get_Words($para[$j]);
  my $nw = $#wrd + 1;
  print "Para $j - $nw\n";
  if ($min > $nw) { $min = $nw; }
  if ($max < $nw) { $max = $nw; }
  $avg = $avg + $nw;

  my $sid = &Get_ID($j);
  my $cln = $para[$j];
  &Make_SingleSpace(\$cln);
  &Handle_Quote(\$cln);
  print fp_prm "( emma_$sid \"$cln\") \n";
  
}
close(fp_prm);
$avg = $avg / ($#para + 1);
$avg = int($avg);

print "Min / Max: $min / $max ; Avg: $avg\n";

sub Get_ID() {

  my $id = shift(@_);
  my $rv = "";
  if ($id < 10) {
    $rv = "000";
  }elsif ($id < 100) {
    $rv = "00";
  } elsif ($id < 1000) {
    $rv = "0";
  }
  $rv = $rv.$id;
  return $rv;
}

sub Handle_Quote() {
   chomp(${$_[0]});
   ${$_[0]} =~ s/[\"]/\\"/g;
}

sub Make_SingleSpace() {
   chomp(${$_[0]});
   ${$_[0]} =~ s/[\s]+$//;
   ${$_[0]} =~ s/^[\s]+//;
   ${$_[0]} =~ s/[\s]+/ /g;
   ${$_[0]} =~ s/[\t]+/ /g;
}

sub Check_FileExistence() {
  my $inF = shift(@_); 
  if (!(-e $inF)) { 
    print "Cannot open $inF \n";
    exit;
  } 
  return 1;
}

sub Get_Lines() {
  my $inF = shift(@_); 
  &Check_FileExistence($inF);
  open(fp_llr, "<$inF");
  my @dat = <fp_llr>;
  close(fp_llr);
  return @dat;
}

sub Get_Words() {
  my $ln = shift(@_);
  &Make_SingleSpace(\$ln);
  my @wrd = split(/ /, $ln);
  return @wrd;
}

sub Get_ProcessedLines() {
  my $inF = shift(@_);
  &Check_FileExistence($inF);
  open(fp_llr, "<$inF");
  my @dat = <fp_llr>;
  close(fp_llr);

  my @nd;
  for (my $i = 0; $i <= $#dat; $i++) {
     my $tl = $dat[$i];
     &Make_SingleSpace(\$tl);
     $nd[$i]  = $tl;
  }
  return @nd;
}
