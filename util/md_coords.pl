#! /usr/bin/perl
# $Id: md_coords.pl.in,v 1.19 2010-07-21 21:51:57 twu Exp $

#$package_version = "2024-02-22";

use warnings;

use IO::File;
undef($opt_9);			# debug mode

undef($opt_c);			# Columns
undef($opt_o);			# Output file
use Getopt::Std;
getopts("9c:o:");

# Usage: md_coords [-o <outfile>] <mdfile>

$mdfile = $ARGV[0];
if (!defined($outfile = $opt_o)) {
  $outfile = "coords.txt";
  $strainfile = "strains.txt";
} elsif ($outfile =~ /coords\.(\S+)/) {
  $genome = $1;
  $strainfile = "strains.$genome";
}

$flags = "";
$flags .= "-o $outfile";

if (defined($opt_c)) {
  ($contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol) = split ",",$opt_c;
} else {
  ($ncols,$examples,$maxwidth) = show_lines($mdfile);
  ($contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol,$include_unmapped_p) = grok_columns($ncols,$examples,$maxwidth,$mdfile);
}
$flags .= " -c $contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol";

($altstrainp,$strains,$refstrain) = find_reference_strain($mdfile,$straincol);
$nstrains = $#{$strains} + 1;
if ($nstrains > 1 && $altstrainp == 1) {
  $firstpass = parse_md_file($mdfile,$contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol,
			     $altstrainp,$refstrain);
  ($altstrains,$separate_chromosome,$treat_as_reference) = check_strains($mdfile,$chrcol,$chrstartcol,$chrendcol,
									 $straincol,$altstrainp,$refstrain,$firstpass);
  $naltstrains = $#{$altstrains} + 1;
  if ($naltstrains == 0) {
    # print STDOUT "No alternate strains\n";
    $altstrainp = 0;
  } else {
    print STDOUT "Remaining non-reference strains: " . join(", ",@ {$altstrains}) . "\n";
    print STDOUT "Would you like to rename any of these strains for GMAP?\n";
    if (input_yn("n") == 1) {
      rename_strains($altstrains);
    }
  }
}


$output = parse_md_file($mdfile,$contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol,
			$altstrainp,$refstrain,$include_unmapped_p,$separate_chromosome,$treat_as_reference);
if ($#{$output} < 0) {
  printf STDOUT "Error: No contigs were read in.\n";
  exit(9);
}

$OUT = new IO::File(">$outfile") or die "Cannot write to file $outfile";
print $OUT "# Reference strain: $refstrain\n";
print $OUT "# To rename a chromosome, edit each occurrence of that chromosome in the gmap_coordinates\n";
print $OUT "# The strain column has information copied from NCBI md files, but is not otherwise used by gmap_setup\n";
print $OUT "# To exclude a contig, place a '#' sign at the beginning of the line\n";
print $OUT "#contig" . "\t" . "gmap_coordinates" . "\t" . "strain\n";
foreach $string (@ {$output}) {
  print $OUT $string;
}
close($OUT);

print STDOUT "\n";

if ($outfile =~ /coords\.(\S+)/) {
  $genome = $1;
  $makefile = "Makefile.$genome";
} else {
  $makefile = "Makefile";
}

print STDOUT "============================================================\n";
print STDOUT "Contig mapping information has been written to file $outfile\n";
print STDOUT "You should look at this file, and edit it if necessary.\n\n";

print STDOUT "Then you should proceed by running\n";
if ($outfile =~ /coords\.(\S+)/) {
  $genome = $1;

  print STDOUT "    make -f Makefile.$genome gmapdb\n";
} else {
  print STDOUT "    make gmapdb\n";
}
print STDOUT "============================================================\n";

exit;


#sub print_strain {
#  my ($FP, $strain) = @_;
#
#  if (defined($newname{$strain})) {
#    print $FP $newname{$strain};
#  } else {
#    print $FP $strain;
#  }
#  return;
#}


sub extract_chr {
  my ($chrfield) = @_;

  if ($chrfield =~ /(\S+)\|/) {
    $chr = $1 . "U";
    $mappedp = 0;
  } else {
    $chr = $chrfield;
    $mappedp = 1;
  }

  return ($chr, $mappedp);
}


sub parse_md_file {
  my ($mdfile, $contigcol, $chrcol, $chrstartcol, $chrendcol, $dircol, $straincol,
      $altstrainp, $refstrain, $include_unmapped_p, $separate_chromosome, $treat_as_reference) = @_;
  my ($FP, $line, $dir, $strain);
  my @output = ();
  my %chrend = ();		# Need to reset, in case this procedure is run again

  $FP = new IO::File("$mdfile") or die "Can't open file $mdfile";
  while (defined($line = <$FP>)) {
    if ($line =~ /^\#/) {
      # Skip
    } else {
      $line =~ s/\r\n/\n/;
      chop $line;
      @fields = split /\t/,$line;
      if (!defined($contig = $fields[$contigcol-1])) {
	die "Can't parse contig in $line";
      }

      $dir = $fields[$dircol-1];
      if (!defined($dir)) {
	print STDERR "No direction for contig $contig.  Treating as forward.\n";
	$dir = "+";
      }

      $strain = $fields[$straincol-1];
      if (!defined($strain)) {
	print STDERR "No strain in line $line.  Treating as reference strain.\n";
	$strain = $refstrain;
      } elsif (defined($treat_as_reference) && defined($ {$treat_as_reference}{$strain})) {
	$strain = $refstrain;
      }

      if (defined($separate_chromosome) && defined($newchr = $ {$separate_chromosome}{$strain})) {
	$chr = $newchr;
	$mappedp = 1;
      } else {
	($chr,$mappedp) = extract_chr($fields[$chrcol-1]);
      }

      if ($mappedp == 0) {
	$seglength = $fields[$chrendcol-1]-$fields[$chrstartcol-1]+1;
	if (!defined($chrend{$chr})) {
	  $chrend{$chr} = 0;
	}
	$chrstart = $chrend{$chr} + 1;
	$chrend = $chrend{$chr} + $seglength;
	$chrend{$chr} = $chrend;

      } else {
	$chrstart = $fields[$chrstartcol-1];
	$chrend = $fields[$chrendcol-1];
      }	
      if ($dir eq "+" || $dir eq "0") {
	$chrinfo = "$chr:$chrstart..$chrend";
      } elsif ($dir eq "-") {
	$chrinfo = "$chr:$chrend..$chrstart";
      } else {
	print STDERR "Cannot understand direction $dir for contig $contig.  Treating as +\n";
	$chrinfo = "$chr:$chrstart..$chrend";
      }

      if (!defined($chr)) {
	# Skip
      } elsif ($chrend == $chrstart) {
	# Skip
      } elsif (!defined($strain)) {
	# Shouldn't get here
	$string = "$contig\t$chrinfo{$contig}\t\n";
	if ($mappedp == 0 && $include_unmapped_p == 0) {
	    push @output,"#" . $string;
	} else {
	    push @output,$string;
	}
      } elsif (!defined($refstrain)) {
	$string = "$contig\t$chrinfo\t\n";
	if ($mappedp == 0 && $include_unmapped_p == 0) {
	    push @output,"#" . $string;
	} else {
	    push @output,$string;
	}
      } elsif ($altstrainp == 0 && $strain ne $refstrain) {
	if (defined($newname{$strain})) {
	  $string = "$contig\t$chrinfo\t$newname{$strain}\n";
	} else {
	  $string = "$contig\t$chrinfo\t$strain\n";
	}
	push @output,"#" . $string;
      } else {
	if (defined($newname{$strain})) {
	  $string = "$contig\t$chrinfo\t$newname{$strain}\n";
	} else {
	  $string = "$contig\t$chrinfo\t$strain\n";
	}
	if ($mappedp == 0 && $include_unmapped_p == 0) {
	    push @output,"#" . $string;
	} else {
	    push @output,$string;
	}
      }
    }
  }

  close($FP);

  return \@output;
}



sub show_lines {
  my ($mdfile) = @_;
  my @lines = ();
  my @examples = ();
  my $ncols = 0;
  my $lineno = 1;

  $FP = new IO::File("$mdfile") or die "Cannot open file $mdfile";
  while (defined($line = <$FP>)) {
    if ($line =~ /^\#/) {
      # Skip
    } else {
      $line =~ s/\r\n/\n/;
      chop $line;
      push @lines,$line;
      @fields = split /\t/,$line;
      if ($#fields+1 > $ncols) {
	if ($ncols != 0) {
	  printf STDERR ("Warning: Line number $lineno has %d cols, but previous lines had %d cols\n",$#fields+1,$ncols);
	}
	$ncols = $#fields+1;
      } elsif ($#fields+1 < $ncols) {
	printf STDERR ("Warning: Line number $lineno has %d cols, but previous lines had %d cols\n",$#fields+1,$ncols);
      }
    }
    $lineno++;
  }
  $nlines = $#lines+1;
  close($FP);

  for ($i = 0; $i < $ncols; $i++) {
    $maxwidth[$i] = 0;
  }

  if ($nlines < 6) {
    for ($i = 0; $i < $nlines; $i++) {
      $line = $lines[$i];
      push @examples,$line;
      @fields = split /\t/,$line;
      for ($j = 0; $j < $#fields; $j++) {
	if (length($fields[$j]) > $maxwidth[$j]) {
	  $maxwidth[$j] = length($fields[$j]);
	}
      }
    }
  } else {
    for ($i = 0; $i < 3; $i++) {
      $line = $lines[$i];
      push @examples,$line;
      @fields = split /\t/,$line;
      for ($j = 0; $j < $#fields; $j++) {
	if (length($fields[$j]) > $maxwidth[$j]) {
	  $maxwidth[$j] = length($fields[$j]);
	}
      }
    }

    for ($i = $#lines-3; $i < $nlines; $i++) {
      $line = $lines[$i];
      push @examples,$lines[$i];
      @fields = split /\t/,$line;
      for ($j = 0; $j < $#fields; $j++) {
	if (length($fields[$j]) > $maxwidth[$j]) {
	  $maxwidth[$j] = length($fields[$j]);
	}
      }
    }
  }

  return ($ncols, \@examples, \@maxwidth);
}


sub print_examples {
  my ($ncols, $examples, $maxwidth) = @_;
  my $line;

  print STDOUT "Here are the first and last few lines of $mdfile:\n\n";
  for ($j = 0; $j < $ncols; $j++) {
    if ($j > 0) {
      print STDOUT " | ";
    }
    $string = sprintf("%d",$j+1);
    printf STDOUT ("%*s",$ {$maxwidth}[$j],$string);
  }
  print "\n";

  for ($j = 0; $j < $ncols; $j++) {
    if ($j > 0) {
      print STDOUT "---";
    }
    for ($k = 0; $k < $ {$maxwidth}[$j]; $k++) {
      print STDOUT "-";
    }
  }
  print "\n";

  foreach $line (@ {$examples}) {
    @fields = split /\t/,$line;
    for ($j = 0; $j < $ncols; $j++) {
      if ($j > 0) {
	print STDOUT " | ";
      }
      printf STDOUT ("%*s",$ {$maxwidth}[$j],$fields[$j]);
    }
    print "\n";
  }

  return;
}


sub n_numeric_fields {
  my ($fields) = @_;
  my ($field);
  my $nnumeric = 0;

  foreach $field (@ {$fields}) {
    if ($field =~ /^\d+$/) {
      $nnumeric++;
    }
  }
  return $nnumeric;
}

sub duplicate_numbers_p {
  my ($fields) = @_;
  my ($field);
  my %seenp;

  foreach $field (@ {$fields}) {
    if ($field =~ /^\d+$/) {
      if (defined($seenp{$field})) {
	return 1;
      } else {
	$seenp{$field} = 1;
      }
    }
  }
  return 0;
}


sub guess_columns {
  my ($mdfile, $ncols) = @_;
  my ($FP, $line, $col, $contigcol, $maxcol, $field);
  my $contig_ambiguous_p = 0;
  my $nlines = 0;
  my @fields;
  my @numericp = ();
  my @alphanumericp = ();
  my @maxvalue = ();
  my @maxlength = ();
  my @valuecount = ();

  $FP = new IO::File("$mdfile") or die "Cannot open file $mdfile";
  while (defined($line = <$FP>)) {
    if ($line !~ /^\#/) {
      $line =~ s/\r\n/\n/;
      chop $line;
      @fields = split /\t/,$line;
      $duplicateline = duplicate_numbers_p(\@fields);
      $nnumeric = n_numeric_fields(\@fields);
      for ($col = 0; $col <= $#fields; $col++) {
	$field = $fields[$col];
	$field =~ s/\|.+//; # For fields like 1|NT_123456
	if (!defined($numericp[$col])) {
	  $numericp[$col] = $alphanumericp[$col] = 0;
	}

	if ($field =~ /^\s*[+-0]\s*$/) {
	  $nplusminus[$col] += 1;

	} elsif ($field =~ /^\d+$/) {
	  $numericp[$col] = 1;
	  if ($nnumeric >= 2 && $duplicateline == 0) {
	    if (!defined($maxvalue[$col])) {
	      $maxvalue[$col] = $field;
	    } elsif ($field > $maxvalue[$col]) {
	      $maxvalue[$col] = $field;
	    }
	  }

	} else {
	  $alphanumericp[$col] = 1;
	  if (!defined($nvalues[$col])) {
	    $nvalues[$col] = 0;
	  }

	  if (!defined($ {$valuecount[$col]}{$field})) {
	    $nvalues[$col] += 1;
	    $ {$valuecount[$col]}{$field} = 0;
	  }
	  $ {$valuecount[$col]}{$field} += 1;
	}

	if (!defined($maxlength[$col])) {
	  $maxlength[$col] = length($field);
	} elsif (length($field) > $maxlength[$col]) {
	  $maxlength[$col] = length($field);
	}
	
	if ($field =~ /^N[A-Z]_\d/) {
	  if (!defined($contigcol)) {
	    $contigcol = $col;
	  } elsif ($col != $contigcol) {
	    $contig_ambiguous_p = 1;
	  }
	}
      }
      $nlines++;
    }
  }
  close($FP);

  if ($contig_ambiguous_p == 1) {
    undef $contigcol;
  }

  # Find chromosomal end (column with highest numeric value)
  $maxcol = -1;
  $maxvalue = 0;
  for ($col = 0; $col < $ncols; $col++) {
    if ($numericp[$col] == 1 && $alphanumericp[$col] == 0) {
      if (defined($maxvalue[$col]) && $maxvalue[$col] > $maxvalue) {
	$maxvalue = $maxvalue[$col];
	$maxcol = $col;
      }
    }
  }
  if (defined($opt_9)) {
    print STDERR "Column with maximum numeric value ($maxvalue) is $maxcol\n";
  }
  $chrendcol = $maxcol;

  # Find chromosomal start (column with second highest numeric value)
  $secondcol = -1;
  $secondvalue = 0;
  for ($col = 0; $col < $ncols; $col++) {
    if ($col != $maxcol && $numericp[$col] == 1 && $alphanumericp[$col] == 0) {
      if (defined($maxvalue[$col]) && $maxvalue[$col] > $secondvalue) {
	$secondvalue = $maxvalue[$col];
	$secondcol = $col;
      }
    }
  }
  if (defined($opt_9)) {
    print STDERR "Column with second highest numeric value ($secondvalue) is $secondcol\n";
  }
  $chrstartcol = $secondcol;

  # Find chromosome (before start column)
  if ($chrstartcol > 0 && $chrstartcol != $chrendcol - 1) {
    if (defined($opt_9)) {
      print STDERR "Undefining columns because $chrstartcol and $chrendcol don't make sense\n";
    }
    undef $chrcol;
    undef $chrstartcol;
    undef $chrendcol;
  } else {
    $chrcol = $chrstartcol - 1;

    # Find strain (column with most distinct values, but not 1 value per line)
    $maxnvaluescol = -1;
    $maxnvalues = 1;
    for ($col = 0; $col < $ncols; $col++) {
      if (!defined($nplusminus[$col])) {
	if ((!defined($contigcol) || $col != $contigcol) && 
	    $alphanumericp[$col] == 1 && $numericp[$col] == 0) {
	  if ($nvalues[$col] < 0.5*$nlines) {
	    if ($nvalues[$col] > $maxnvalues) {
	      $maxnvalues = $nvalues[$col];
	      $maxnvaluescol = $col;
	    }
	  }
	}
      }
    }
    $straincol = $maxnvaluescol;
  }

  $maxplusminus = 0;
  for ($col = 0; $col < $ncols; $col++) {
    if (defined($nplusminus[$col]) && $nplusminus[$col] > $maxplusminus) {
      $maxplusminus = $nplusminus[$col];
      $dircol = $col;
    }
  }

  # Make columns 1-based
  if (defined($contigcol)) {
    $contigcol++;
  }
  if (defined($chrcol)) {
    $chrcol++;
  }
  if (defined($chrstartcol)) {
    $chrstartcol++;
  }
  if (defined($chrendcol)) {
    $chrendcol++;
  }
  if (defined($dircol)) {
    $dircol++;
  }
  if (defined($straincol)) {
    $straincol++;
  }

  return ($contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol);
}


sub grok_columns {
  my ($ncols, $examples, $maxwidth, $mdfile) = @_;
  my @fields;
  my $unmapped_example;
  my $include_unmapped_p;

  print STDERR "Parsing contig file $mdfile...\n";
  ($contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol) = guess_columns($mdfile,$ncols);

  print_examples($ncols,$examples,$maxwidth);

  $donep = 0;
  while ($donep == 0) {
    printf STDOUT "Which column has the contig name? (These must exist in header lines of the FASTA files, e.g., NT_077402)\n";
    $contigcol = input_numeric(1,$ncols,$contigcol);
    @results = ();
    foreach $example (@ {$examples}) {
      @fields = split /\t/,$example;
      push @results,$fields[$contigcol-1];
    }
    #printf STDOUT "Contig names are: " . join(",",@results) . "\n";
    #print STDOUT "Is this correct (y or n)?\n";
    #$donep = input_yn("y");
    $donep = 1;
  }

  $donep = 0;
  while ($donep == 0) {
    printf STDOUT "Which column has the chromosome name (e.g., 1, 22, X, Y|NT_099038)?\n";
    $chrcol = input_numeric(1,$ncols,$chrcol);
    @results = ();
    foreach $example (@ {$examples}) {
      @fields = split /\t/,$example;
      ($chr,$mappedp) = extract_chr($fields[$chrcol-1]);
      if ($mappedp == 0 && !defined($unmapped_example)) {
	  $unmapped_example = $fields[$chrcol-1];
      }
      push @results,$chr;
    }
    #printf STDOUT "Chromosome names are: " . join(",",@results) . "\n";
    #print STDOUT "Is this correct (y or n)?\n";
    #$donep = input_yn("y");
    $donep = 1;
  }

  if (!defined($unmapped_example)) {
      $include_unmapped_p = 1;
  } else {
      print STDOUT "Some contigs are unmapped (e.g., $unmapped_example).  Include unmapped contigs?\n";
      $include_unmapped_p = input_yn("y");
  }


  $donep = 0;
  while ($donep == 0) {
    printf STDOUT "Which column has the chromosomal start position?\n";
    $chrstartcol = input_numeric(1,$ncols,$chrstartcol);
    @results = ();
    foreach $example (@ {$examples}) {
      @fields = split /\t/,$example;
      push @results,$fields[$chrstartcol-1];
    }
    #printf STDOUT "Chromosomal start positions are: " . join(",",@results) . "\n";
    #print STDOUT "Is this correct (y or n)?\n";
    #$donep = input_yn("y");
    $donep = 1;
  }

  $donep = 0;
  while ($donep == 0) {
    printf STDOUT "Which column has the chromosomal end position?\n";
    $chrendcol = input_numeric(1,$ncols,$chrendcol);
    @results = ();
    foreach $example (@ {$examples}) {
      @fields = split /\t/,$example;
      push @results,$fields[$chrendcol-1];
    }
    #printf STDOUT "Chromosomal end positions are: " . join(",",@results) . "\n";
    #print STDOUT "Is this correct (y or n)?\n";
    #$donep = input_yn("y");
    $donep = 1;
  }

  $donep = 0;
  while ($donep == 0) {
    printf STDOUT "Which column has the contig direction (+ or -)?\n";
    $dircol = input_numeric(1,$ncols,$dircol);
    @results = ();
    foreach $example (@ {$examples}) {
      @fields = split /\t/,$example;
      push @results,$fields[$dircol-1];
    }
    #printf STDOUT "Chromosomal end positions are: " . join(",",@results) . "\n";
    #print STDOUT "Is this correct (y or n)?\n";
    #$donep = input_yn("y");
    $donep = 1;
  }

  $donep = 0;
  while ($donep == 0) {
    printf STDOUT "Which column has the strain information?\n";
    $straincol = input_numeric(1,$ncols,$straincol);
    @results = ();
    foreach $example (@ {$examples}) {
      @fields = split /\t/,$example;
      push @results,$fields[$straincol-1];
    }
    #printf STDOUT "Strains are: " . join(",",@results) . "\n";
    #print STDOUT "Is this correct (y or n)?\n";
    #$donep = input_yn("y");
    $donep = 1;
  }

  return ($contigcol,$chrcol,$chrstartcol,$chrendcol,$dircol,$straincol,$include_unmapped_p);
}


sub rename_strains {
  my ($strains) = @_;

  while (1) {
    print STDOUT "Strain to rename (choose from below, press return to quit)?\n";
    if (!defined($strain = input_choices($strains,0))) {
      return;
    } else {
      print STDOUT "New name for this strain?\n";
      $newname{$strain} = input_nonempty();
    }
  }

  return;
}


sub find_reference_strain {
  my ($mdfile, $straincol) = @_;

  $FP = new IO::File("$mdfile") or die "Cannot open file $mdfile";
  while (defined($line = <$FP>)) {
    if ($line !~ /^\#/) {
      $line =~ s/\r\n/\n/;
      chop $line;
      @fields = split /\t/,$line;
      $strain = $fields[$straincol-1];

      if (!defined($straincount{$strain})) {
	$straincount{$strain} = 0;
	$strainlength{$strain} = 0;
      }
      $straincount{$strain} += 1;
      $strainlength{$strain} += $fields[$chrendcol-1] - $fields[$chrstartcol-1];
    }
  }
  close($FP);

  @strains = keys %straincount;

  $nstrains = $#strains + 1;
  if ($nstrains == 0) {
    print STDOUT "No strains detected\n";
    return (0,\@strains);

  } elsif ($nstrains == 1) {
    print STDOUT "One strain detected: $strains[0]\n";
    print STDOUT "Would you like to rename this strain for GMAP?\n";
    if (input_yn("n") == 1) {
      rename_strains(\@strains);
    }
    return (0,\@strains,$strains[0]);

  } else {
    # Find reference strain (value with highest count)
    $refstrain = "";
    $refstrainlength = 0;
    foreach $strain (@strains) {
      if ($strainlength{$strain} > $refstrainlength) {
	$refstrainlength = $strainlength{$strain};
	$refstrain = $strain;
      }
    }

    print STDOUT "$nstrains apparent \"strains\" detected:\n";
    foreach $strain (@strains) {
      print STDOUT "   $strain ($straincount{$strain} contigs, $strainlength{$strain} total nucleotides)\n";
    }

    # print STDOUT "Include multiple strains (y or n)?\n";
    if (1 || input_yn("y") != 1) {
      printf STDOUT ("Strain with the most nucleotides is '%s'.  Is this the one to use as the reference strain?\n",$refstrain);
      $input = input_yn("y");
      if ($input == 1) {
	return (0,\@strains,$refstrain);
      } else {
	while (1) {
	  print STDOUT "Which strain is the reference (choose from below)?\n";
	  $refstrain = input_choices(\@strains,1);
	  if (defined($straincount{$refstrain})) {
	    return (0,\@strains,$refstrain);
	  }
	}
      }


    } else {
      printf STDOUT ("Strain with the most lines is '%s'.  Is this the reference strain?\n",$refstrain);
      $input = input_yn("y");
      if ($input == 1) {
	return (1,\@strains,$refstrain);
      } else {
	while (1) {
	  print STDOUT "Which strain is the reference (choose from below)?\n";
	  $refstrain = input_choices(\@strains,1);
	  if (defined($straincount{$refstrain})) {
	    return (1,\@strains,$refstrain);
	  }
	}
      }
    }
  }
}


sub chrinfo_overlap {
  my ($chrinfo1, $chrinfo2) = @_;

  ($chr1,$start1,$end1) = $chrinfo1 =~ /(\S+):(\d+)\.\.(\d+)/;
  ($chr2,$start2,$end2) = $chrinfo2 =~ /(\S+):(\d+)\.\.(\d+)/;
  if ($chr1 ne $chr2) {
    return 0;
  } else {
    if ($start1 < $end1) {
      $left1 = $start1;
      $right1 = $end1;
    } else {
      $left1 = $end1;
      $right1 = $start1;
    }

    if ($start2 < $end2) {
      $left2 = $start2;
      $right2 = $end2;
    } else {
      $left2 = $end2;
      $right2 = $start2;
    }

    if ($left2 > $right1) {
      return 0;
    } elsif ($left1 > $right2) {
      return 0;
    } else {
      return 1;
    }
  }
}


sub overlapp {
  my ($altstrain, $refstrain, $firstpass) = @_;
  my $line;
  my @fields;

  foreach $line1 (@ {$firstpass}) {
    chomp $line1;
    @fields = split /\t/,$line1;
    $strain1 = $fields[2];
    if ($strain1 eq $altstrain) {
      $chrinfo1 = $fields[1];
      foreach $line2 (@ {$firstpass}) {
	chomp $line2;
	@fields = split /\t/,$line2;
	$strain2 = $fields[2];
	if ($strain2 eq $refstrain) {
	  $chrinfo2 = $fields[1];
	  if (chrinfo_overlap($chrinfo1,$chrinfo2) == 1) {
	    return 1;
	  }
	}
      }
    }
  }
  return 0;
}


sub check_strains {
  my ($mdfile, $chrcol, $chrstartcol, $chrendcol, $straincol, $altstrainp, $refstrain, $firstpass) = @_;
  my @strains = ();
  my @altstrains = ();

  $FP = new IO::File("$mdfile") or die "Cannot open file $mdfile";
  while (defined($line = <$FP>)) {
    if ($line =~ /^\#/) {
      # Skip
    } else {
      $line =~ s/\r\n/\n/;
      chop $line;
      @fields = split /\t/,$line;
      ($chr,$mappedp) = extract_chr($fields[$chrcol-1]);
      $seglength = $fields[$chrendcol-1]-$fields[$chrstartcol-1]+1;
      $existingchrnamep{$chr} = 1;

      $strain = $fields[$straincol-1];
      if (!defined($strain) || $strain eq $refstrain) {
	$referencelength{$chr} += $seglength;
      } else {
	if (!defined($seenstrainp{$strain})) {
	  push @strains,$strain;
	  $seenstrainp{$strain} = 1;
	  % {$seenchrp{$strain}} = ();
	  $nchr{$strain} = 0;
	}
	if (!defined($ {$seenchrp{$strain}}{$chr})) {
	  $nchr{$strain} += 1;
	  $lastchr{$strain} = $chr;
	  $ {$seenchrp{$strain}}{$chr} = 1;
	}
	$altlength{$strain . "_" . $chr} += $seglength;
      }
    }
  }
  close($FP);

  # Collect altstrains or convert to reference
  foreach $strain (@strains) {
    if ($strain eq $refstrain) {
      # Skip
    } elsif ($nchr{$strain} == 1) {
      $chr = $lastchr{$strain};
      if (!defined($referencelength{$chr}) || $referencelength{$chr} == 0) {
	print STDOUT "Alternate strain '$strain' contains a single chromosome: $chr\n";
	print STDOUT "This chromosome does not exist on the reference strain\n";
	print STDOUT "Treat this chromosome as part of the reference strain (y or n)?\n";
	$default = "y";

	if (input_yn($default) == 1) {
	  print STDOUT "Okay, will treat chromosome $chr as part of the reference strain\n";
	  $treat_as_reference{$strain} = 1;
	} elsif ($altstrainp == 0) {
	  print STDOUT "Okay, will ignore contigs belonging to strain $strain\n";
	} else {
	  push @altstrains,$strain;
	}

      } elsif (overlapp($strain,$refstrain,$firstpass) == 0) {
	print STDOUT "Alternate strain '$strain' contains a single chromosome: $chr\n";
	print STDOUT "On this chromosome, this strain does not overlap the reference strain\n";
	print STDOUT "Include this strain as part of the reference strain (y or n)?\n";
	$default = "y";

	if (input_yn($default) == 1) {
	  print STDOUT "Okay, will include strain $strain as part of the reference strain\n";
	  $treat_as_reference{$strain} = 1;
	} elsif ($altstrainp == 0) {
	  print STDOUT "Okay, will ignore contigs belonging to strain $strain\n";
	} else {
	  push @altstrains,$strain;
	}

      } else {
	$coverage = $altlength{$strain . "_" . $chr}/$referencelength{$chr};
	if ($coverage > 0.30) {
	  print STDOUT "Alternate strain '$strain' contains a single chromosome: $chr\n";
	  printf STDOUT ("This chromosome has %.1f%% of the length as the reference strain\n",100.0*$coverage);
	  print STDOUT "Because of its long length, it probably represents an entire alternate version of chromosome $chr,\n";
	  print STDOUT "  rather than a localized strain variant\n";
	  print STDOUT "Treat this strain as part of the reference strain, as an entire alternate version of chromosome $chr (y or n)?\n";
	  $altstrain_answer = 0;

	} elsif ($altstrainp == 0) {
	  # Looks like a localized strain variant, and user doesn't want these, so skip any further actions
	  undef $altstrain_answer;

	} else {
	  print STDOUT "Alternate strain '$strain' contains a single chromosome: $chr\n";
	  printf STDOUT ("This chromosome has %.1f%% of the length as the reference strain\n",100.0*$coverage);
	  print STDOUT "Because of its short length, it probably represents a localized strain variant,\n";
	  print STDOUT "  rather than an entire alternate version of chromosome $chr\n";
	  print STDOUT "Keep this strain as a localized strain variant (y or n)?\n";
	  $altstrain_answer = 1;
	}

	if (!defined($altstrain_answer)) {
	  # Skip any actions
	} elsif (input_yn("y") == $altstrain_answer) {
	  if ($altstrainp == 0) {
	    print STDOUT "Okay, will ignore contigs belonging to strain $strain\n";
	  } else {
	    print STDOUT "Okay, will keep strain $strain as a localized strain variant\n";
	    push @altstrains,$strain;
	  }
	} else {
	  print STDOUT "Okay, will treat strain $strain as part of the reference strain, as an entire alternate version of chromosome $chr\n";
	  undef $existingchrnamep{$chr};
	  $donep = 0;
	  while ($donep == 0) {
	    print STDOUT "Name for this alternate version of chromosome $chr?\n";
	    $newchr = input_nonempty();
	    if (defined($existingchrnamep{$newchr})) {
	      print STDOUT "That chromosome name is already taken.  Try again\n";
	    } else {
	      $donep = 1;
	      $existingchrnamep{$newchr} = 1;
	      $separate_chromosome{$strain} = $newchr;
	    }
	  }
	}
      }
    }
  }

  return (\@altstrains,\%separate_chromosome,\%treat_as_reference);
}


########################################################################


sub input_any {
  while (1) {
    print STDOUT "Response> ";
    $input = <STDIN>;
    chop $input;

    $input =~ s/^\s+//;
    $input =~ s/\s+$//;

    if ($input =~ /\s/) {
      print STDOUT "No spaces are allowed.  Please try again.\n";
    } else {
      return $input;
    }
  }
}



sub input_nonempty {
  while (1) {
    print STDOUT "Response> ";
    $input = <STDIN>;
    chop $input;

    $input =~ s/^\s+//;
    $input =~ s/\s+$//;

    if ($input =~ /\s/) {
      print STDOUT "No spaces are allowed.  Please try again.\n";
    } elsif ($input =~ /\S/) {
      return $input;
    }
  }
}


sub input_yn {
  my ($default) = @_;

  while (1) {
    if (defined($default)) {
      print STDOUT "Response [$default]> ";
    } else {
      print STDOUT "Response> ";
    }
    $input = <STDIN>;
    chop $input;

    $input =~ s/^\s+//;
    $input =~ s/\s+$//;

    if ($input !~ /\S/) {
      $input = $default;
    }

    if ($input =~ /^[Yy]/) {
      return 1;
    } elsif ($input =~ /^[Nn]/) {
      return 0;
    }
  }
}

sub input_numeric {
  my ($min, $max, $default) = @_;

  while (1) {
    if (defined($default)) {
      print STDOUT "Response [$default]> ";
    } else {
      print STDOUT "Response> ";
    }

    $input = <STDIN>;
    chop $input;

    $input =~ s/^\s+//;
    $input =~ s/\s+$//;

    if ($input !~ /\S/) {
      $input = $default;
    }
    if ($input =~ /^\d+$/ && $input >= $min && $input <= $max) {
      return $input;
    } else {
      print STDOUT "Expecting a number between $min and $max.  Please try again.\n";
    }
  }
}

sub input_letter {
  my ($min, $max) = @_;

  while (1) {
    print STDOUT "Response> ";
    $input = <STDIN>;
    chop $input;

    $input =~ s/^\s+//;
    $input =~ s/\s+$//;

    if ($input =~ /^[a-zA-Z]$/ && $input ge $min && $input le $max) {
      return $input;
    } else {
      print STDOUT "Expecting a letter between $min and $max.  Please try again.\n";
    }
  }
}

sub input_choices {
  my ($choices, $requireonep) = @_;

  while (1) {
    print STDOUT "Choices: \n";
    foreach $choice (@ {$choices}) {
      if (defined($newname{$choice})) {
	print STDOUT "  " . $choice . " (to be renamed $newname{$choice})" . "\n";
      } else {
	print STDOUT "  " . $choice . "\n";
      }
    }
    print STDOUT "\n";
    print STDOUT "Response> ";
    $input = <STDIN>;
    chop $input;

    $input =~ s/^\s+//;
    $input =~ s/\s+$//;

    foreach $choice (@ {$choices}) {
      if ($input eq $choice) {
	return $input;
      }
    }
    if ($input !~ /\S/ && $requireonep == 0) {
      return;
    } else {
      print STDOUT "Your input doesn't match any of the choices.  Please try again.\n";
    }
  }
}

