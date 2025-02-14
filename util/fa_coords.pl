#! /usr/bin/perl
# $Id: fa_coords.pl.in,v 1.23 2010-07-21 21:49:33 twu Exp $

#$package_version = "2024-02-22";

use warnings;

use IO::File;
$opt_l = 1000000;		# Don't report contigs smaller than this length
undef($opt_o);			# Output file
undef($opt_C);			# Try to parse chromosomal information
undef($opt_E);			# Interpret argument as a command
undef($opt_g);			# gunzip each file first
#undef($opt_S);			# Do not order chromosomes
undef($opt_c);			# Circular chromosomes
undef($opt_2);			# File with altscaffold info
undef($opt_n);			# contig_hash_file
undef($opt_N);			# contig_order_file
undef($opt_L);			# limit_to_names_p
undef($opt_f);			# File with input FASTA source filenames
undef($opt_Q);			# FASTQ files
undef($opt_R);			# Reverse all reads
use Getopt::Std;
getopts("o:CEgc:2:n:N:Lf:QR");

# Usage: fa_coords [-o <output>] [-C] [-E] [-g] [-n <namefile>] [-f <sourcefiles>]

if (defined($circular = $opt_c)) {
    if (-e $circular && -f $circular) {
	print STDERR "Reading circular chromosomes from the file $circular...";
	$ncircular = 0;
	$FP = new IO::File($circular);
	while (defined($line = <$FP>)) {
	    ($chr) = $line =~ /(\S+)/;
	    $circularp{$chr} = 1;
	    $ncircular++;
	}
	close($FP);
	print STDERR "read $ncircular circular chromosomes\n";
    } else {
	foreach $chr (split ",",$circular) {
	    $circularp{$chr} = 1;
	}
    }
}

if (defined($altscaffold = $opt_2)) {
    if (-e $altscaffold && -f $altscaffold) {
	print STDERR "Reading altscaffold info from the file $altscaffold...";
	$naltloc = 0;
	$FP = new IO::File($altscaffold);
	while (defined($line = <$FP>)) {
	    chop $line;
	    ($alt_scaf_acc,$primary_chr,$orientation,$alt_scaf_start,$alt_scaf_stop,$primary_start,$primary_end) = split /\t/,$line;
	    if (!defined($alt_scaf_acc)) {
		die $line;
	    }
	    # Cannot include a space inside of the string
	    $altscaffold_info{$alt_scaf_acc} = "$alt_scaf_start..$alt_scaf_stop,$primary_chr:$primary_start..$primary_end";
	    $naltloc++;
	}
	close($FP);
	print STDERR "read $naltloc alternate scaffolds\n";
    } else {
	die "Could not find file $altscaffold";
    }
}

if (!defined($outfile = $opt_o)) {
    $outfile = "coords.txt";
}

if (defined($opt_n)) {
    # Expecting 2-column format
    $FP = new IO::File($opt_n) or die;
    while (defined($line = <$FP>)) {
	chop $line;
	($old,$new) = split /\s+/,$line;
	if (defined($old) && defined($new) && $new =~ /\S/) {
	    $contig_newname{$old} = $new;
	}
    }
    close($FP);

} elsif (defined($opt_N)) {
    # Expecting 1-column format
    $FP = new IO::File($opt_N) or die;
    @ {$contig_newnames_byorder} = ();
    while (defined($line = <$FP>)) {
	($new) = $line =~ /(\S+)/;
	push @ {$contig_newnames_byorder},$new;
    }
    close($FP);
    $contig_i = 0;
}

if (defined($opt_L)) {
    $limit_to_names_p = 1;
} else {
    $limit_to_names_p = 0;
}    


$flags = "";
$flags .= "-o $outfile";

#print STDERR "Printing processing messages only for those contigs longer than\n";
#print STDERR "  $opt_l nt (but processing all of them, of course).\n";
#print STDERR "  You can change this value with the -l flag to fa_coords.\n";

if (defined($opt_f)) {
    # Source files given in a file
    @sourcefiles = ();
    $SOURCES = new IO::File($opt_f) or die "Cannot open file $opt_f";
    while (defined($line = <$SOURCES>)) {
	chop $line;
	push @sourcefiles,$line;
    }
    close($SOURCES);
    if (defined($opt_Q)) {
	($output,$skipped) = parse_fq_files(\@sourcefiles);
    } else {
	($output,$skipped) = parse_fa_files(\@sourcefiles);
    }

} elsif ($#ARGV < 0) {
    # FASTA is piped via stdin
    @streams = ();
    push @streams,"<&STDIN";
    if (defined($opt_Q)) {
	($output,$skipped) = parse_fq_files(\@streams);
    } else {
	($output,$skipped) = parse_fa_files(\@streams);
    }

} else {
    # Source files given on command line
    if (defined($opt_Q)) {
	($output,$skipped) = parse_fq_files(\@ARGV);
    } else {
	($output,$skipped) = parse_fa_files(\@ARGV);
    }
}

if ($#{$output} < 0) {
    printf STDOUT "Error: No contigs were read in.\n";
    exit(9);
} else {
    $OUT = new IO::File(">$outfile") or die "Cannot write to file $outfile";
    print $OUT "# To rename a chromosome, edit each occurrence of that chromosome in the gmap_coordinates\n";
    print $OUT "# The strain column has information copied from NCBI md files, but is not otherwise used by gmap_setup\n";
    print $OUT "# To exclude a contig, place a '#' sign at the beginning of the line\n";
    print $OUT "# The <primary> field means the primary segment for the given (altloc) contig\n";
    print $OUT "#contig" . "\t" . "gmap_coordinates" . "\t" . "linear/circular/<primary>" . "\t" . "strain\n";

    #if (defined($opt_S)) {
    #@sorted = @ {$output};
    #} else {
    #@sorted = sort by_numeric_alpha (@ {$output});
    #}
    @sorted = @ {$output};
    foreach $string (@sorted) {
	print $OUT $string;
    }
    close($OUT);
}

if ($#$skipped >= 0) {
    printf "\n";
    printf STDOUT "********************************************************************************\n";
    printf STDOUT ("    A total of %d contigs had no recognizable chromosomal assignment, and\n",$#$skipped + 1);
    print <<NAMSG;
were therefore concatenated into a chromosome called "NA".

GMAP can handle this chromosome without any problems.  However,
if you do not wish to include these contigs, please remove them from $outfile,
or comment them out with the # character at the beginning of each line.
NAMSG

    printf STDOUT "********************************************************************************\n";
    print "\n";
}

@errors = ();
foreach $chr (keys %lowest) {
    if ($lowest{$chr} > 1) {
	push @errors,"  First contig in chromosome $chr starts at position $lowest{$chr}";
    }
}

if ($#errors >= 0) {
    print STDOUT "\n";
    print STDOUT "*** Possible errors: ***\n";
    foreach $error (@errors) {
	print $error . "\n";
    }
    print STDOUT "\n";
    print STDOUT "  Some the errors above may be addressed by specifying the contigs to be on\n";
    print STDOUT "  alternate strains of existing chromosomes, rather than on independent\n";
    print STDOUT "  alternate chromosomes.\n";
    print STDOUT "  You may make the appropriate changes in $outfile, by adding an alternate\n";
    print STDOUT "  strain in column 3, and specifying an existing chromosome in column 2\n";
    print STDOUT "\n";
}

print STDOUT "\n";
print STDOUT "============================================================\n";
print STDOUT "Contig mapping information has been written to file $outfile.\n";
if ($#errors >= 0) {
    printf STDOUT ("%d possible errors were found (listed above)\n",$#errors+1);
}

#print STDOUT "You should look at this file, and edit it if necessary\n";
#print STDOUT "If everything is okay, you should proceed by running\n";
#if ($outfile =~ /coords\.(\S+)/) {
#    $genome = $1;
#
#    print STDOUT "    make -f Makefile.$genome gmapdb\n";
#} else {
#    print STDOUT "    make gmapdb\n";
#}
print STDOUT "============================================================\n";

exit;



sub handle_consecutive_errors {
    my ($consec_errors) = @_;

    if ($consec_errors > 0) {
	print STDOUT "\n\n";
	print STDOUT "***  Note: For a total of $consec_errors consecutive contigs, ";
	print STDOUT "the chromosome could not be parsed from the header, \n";
	print STDOUT "     and were therefore assigned to chromosome NA.\n\n";
	$consec_errors = 0;
    }
    return $consec_errors;
}



sub parse_fa_files {
    my ($argv) = @_;
    my ($FP, $line, $strain);
    my @output = ();
    my @skipped = ();
    my $seglength;
    my $chronlyp;		# No contig coordinates given, only a chromosome name
    my $contig_oldname;		# Old name
    my $contig;			# New name
    my $nwarnings_dup = 0;
    my $nwarnings_colon = 0;
    my $ncontigs = 0;

    foreach $arg (@ {$argv}) {
	if (defined($opt_E)) {
	    printf STDOUT "Executing command $arg\n";
	    $FP = new IO::File("$arg |") or die "Can't execute $arg";
	} elsif (defined($opt_g)) {
	    printf STDOUT "Opening gzipped file $arg\n";
	    $FP = new IO::File("gunzip -c \"$arg\" |") or die "Can't execute $arg";
	} else {
	    printf STDOUT "Opening file $arg\n";
	    $FP = new IO::File("$arg") or die "Can't open file $arg";
	}
	$seglength = 0;
	undef($orientation);
	$consec_errors = 0;
	$shortp = 0;
	while (defined($line = <$FP>)) {
	    $line =~ s/\r\n/\n/;
	    chomp $line;
	    if ($line !~ /\S/) {
		# Skip blank lines

	    } elsif ($line !~ /^>/) {
		if (defined($chr)) {
		    $seglength += length($line);
		}

	    } else {
		# Handle previous contig
		if (!defined($contig)) {
		    # No previous contig

		} elsif (defined($seglength{$contig})) {
		    if ($seglength != $seglength{$contig}) {
			die "Saw contig $contig already, and second length $seglength is different from first $seglength{$contig}";
		    } elsif ($nwarnings_dup < 50) {
			print STDOUT "Saw contig $contig already.  Ignoring previous occurrence.\n";
		    } elsif ($nwarnings_dup == 50) {
			print STDERR "More than 50 warnings.  Will stop printing warnings\n";
		    }
		    $nwarnings_dup++;

		} elsif ($seglength > 0) {
		    $seglength{$contig} = $seglength;
		    if ($seglength > $opt_l) {
			if ($shortp == 1) {
			    print STDOUT "\n";
			    $shortp = 0;
			}
			if ($chronlyp == 1) {
			    printf STDOUT ("  Contig %s: concatenated at chromosome end: ",$contig);
			} else {
			    printf STDOUT ("  Contig %s: parsed chromosomal coordinates: ",$contig);
			}
			if (defined($chr_newname{$chr})) {
			    printf STDOUT ("%s:%d..",$chr_newname{$chr},$chrpos{$chr});
			} else {
			    printf STDOUT ("%s:%d..",$chr,$chrpos{$chr});
			}
			printf STDOUT ("%d (length = %d nt)",$chrpos{$chr}+$seglength-1,$seglength);
			if (defined($opt_R)) {
			    printf STDOUT (" (revcomp => %s:%d..%d)",$chr,$chrpos{$chr}+$seglength-1,$chrpos{$chr});
			} elsif (defined($orientation) && $orientation eq "rev") {
			    printf STDOUT (" (revcomp => %s:%d..%d)",$chr,$chrpos{$chr}+$seglength-1,$chrpos{$chr});
			}
			print STDOUT "\n";
		    } else {
			if ($shortp == 0) {
			    printf STDOUT "  Processed short contigs (<$opt_l nt): ";
			    $shortp = 1;
			}
			if (++$ncontigs < 100) {
			    printf STDOUT ".";
			} elsif ($ncontigs == 100) {
			    printf STDOUT "More than 100 short contigs.  Will stop printing.\n";
			}
		    }

		    if (defined($chr_newname{$chr})) {
			$string = sprintf("%s\t%s:",$contig,$chr_newname{$chr});
		    } else {
			$string = sprintf("%s\t%s:",$contig,$chr);
		    }
		    if (defined($opt_R)) {
			$string .= sprintf("%d..%d",$chrpos{$chr}+$seglength-1,$chrpos{$chr});
		    } elsif (defined($orientation) && $orientation eq "rev") {
			$string .= sprintf("%d..%d",$chrpos{$chr}+$seglength-1,$chrpos{$chr});
		    } else {
			$string .= sprintf("%d..%d",$chrpos{$chr},$chrpos{$chr}+$seglength-1);
		    }

		    # Note: using new name for circular argument
		    if (defined($circularp{$contig})) {
			$string .= "\tcircular";
		    } elsif (defined($altscaffold_info{$chr})) {
			$string .= "\t" . $altscaffold_info{$chr};
		    } else {
			$string .= "\tlinear";
		    }
		    $string .= "\n";

		    push @output,$string;
		    if (!defined($lowest{$chr})) {
			$lowest{$chr} = $chrpos{$chr};
		    } elsif ($chrpos{$chr} < $lowest{$chr}) {
			$lowest{$chr} = $chrpos{$chr};
		    }
		    $chrpos{$chr} += $seglength; # Used only when a header doesn't have a chrpos for this chr
		}
		
		# Handle current header
		# print STDOUT "  Header line: $line\n";
		$seglength = 0;
		$chronlyp = 1;  # initialize, meaning we expect not to find any chromosomal coordinates in the header line

		($contig_oldname) = $line =~ /^>(\S+)/;

		if (defined($contig_newnames_byorder)) {
		    print STDERR "Changing name $contig_oldname to $ {$contig_newnames_byorder}[$contig_i]\n";
		    $contig = $ {$contig_newnames_byorder}[$contig_i++];
		} elsif (!defined($opt_n)) {
		    $contig = $contig_oldname;
		} elsif (defined($contig_newname{$contig_oldname})) {
		    print STDERR "Changing name $contig_oldname to $contig_newname{$contig_oldname}\n";
		    $contig = $contig_newname{$contig_oldname};
		} elsif ($limit_to_names_p == 0) {
		    $contig = $contig_oldname;
		} else {
		    print STDERR "Skipping $contig_oldname because not found in $opt_n and --limit-to-names was specified\n";
		    undef $contig;
		}

		# Determine the chromosome and coordinates for the given contig
		undef $orientation;
		undef $chr;
		if (!defined($contig)) {
		    # Skip

		} elsif (!defined($opt_C)) {
		    $chr = $contig;
		    if (0 && $chr =~ /:/) {
			if ($nwarnings_colon < 50) {
			    print STDERR "Replacing : in $chr with _\n";
			} elsif ($nwarnings_colon == 50) {
			    print STDERR "More than 50 warnings.  Will stop printing warnings\n";
			}
			$nwarnings_colon++;
			$chr =~ s/:/_/g;
		    }
		    $chronlyp = 1;
		    
		} elsif ($line =~ /[Cc]hr_(\S+)/) {
		    # Seen in some TIGR contigs
		    $chr = $1;
		    $chronlyp = 1;
		    $consec_errors = handle_consecutive_errors($consec_errors);
		    
		} elsif ($line =~ /[Cc]hr\s*=?\s*(\S+):(\d+)\D+\d+/) {
		    $chr = $1;
		    $chrpos{$chr} = $2;
		    $chronlyp = 0;
		    $consec_errors = handle_consecutive_errors($consec_errors);
		    
		} elsif ($line =~ /[Cc]hromosome\s*(\S+)/) {
		    # NCBI .mfa format
		    $chr = $1;
		    $chr =~ s/[,;:.]$//;
		    $chronlyp = 1;
		    $consec_errors = handle_consecutive_errors($consec_errors);
		    
		} elsif ($line =~ /[Cc]hromosome:[^:]+:([^:]+):(\d+)/) {
		    # Ensembl format: chromosome:NCBI35:22:1:49554710:1
		    $chr = $1;
		    $chrpos{$chr} = $2;
		    $chronlyp = 0;
		    
		    if ($chr =~ /(\S+?)_N\D_\d+/) {
			# Ensembl notation for unmapped contig
			$chr = $1 . "U";
		    }
		    $consec_errors = handle_consecutive_errors($consec_errors);
		    
		} elsif ($line =~ /\/[Cc]hromosome=\S+/) {
		    # Celera format
		    ($chr) = $line =~ /\/[Cc]hromosome=(\S+)/;
		    if ($line =~ /\/alignment=\((\d+)-\d+\)/) {
			$chrpos{$chr} = $1;
			$chrpos{$chr} += 1;	# Because Celera uses 0-based coordinates
			$chronlyp = 0;
		    }
		    if ($line =~ /\/orientation=rev/) {
			$orientation = "rev";
		    }
		    $consec_errors = handle_consecutive_errors($consec_errors);

		} elsif ($line =~ /[Cc]hr\s*(\S+):(\d+)\D+\d+/) {
		    $chr = $1;
		    $chrpos{$chr} = $2;
		    $chronlyp = 0;
		    $consec_errors = handle_consecutive_errors($consec_errors);

		} elsif ($line =~ /[Cc]hr\s*=?\s*(\S+) && $1 ne "omosome"/) {
		    $chr = $1;
		    $chronlyp = 1;
		    $consec_errors = handle_consecutive_errors($consec_errors);

		} elsif ($line =~ /[Cc]hr\s*(\S+)/ && $1 ne "omosome") {
		    $chr = $1;
		    $chronlyp = 1;
		    $consec_errors = handle_consecutive_errors($consec_errors);

		} else {
		    if ($consec_errors == 0) {
			print STDOUT "\n\n***  Note: Can't find chromosome in header $line.  Assigning to chromosome NA instead.\n\n";
		    }
		    $chr = "NA";
		    push @skipped,$contig;
		    $consec_errors += 1;
		}

		if (defined($contig) && !defined($chr)) {
		    if ($consec_errors == 0) {
			print STDOUT "\n\n***  Note: Can't find chromosome in header $line.  Assigning to chromosome NA instead.\n\n";
		    }
		    $chr = "NA";
		    push @skipped,$contig;
		    $consec_errors += 1;
		}

		if (defined($contig) && !defined($chrpos{$chr})) {
		    $chrpos{$chr} = 1;	# Start this contig at beginning of chromosome
		}
	    }
	}

	# Handle last contig in the file
	if (!defined($contig)) {
	    # No previous contig

	} elsif (defined($seglength{$contig})) {
	    if ($seglength != $seglength{$contig}) {
		die "Saw contig $contig already, and second length $seglength is different from first $seglength{$contig}";
	    } else {
		print STDOUT "Saw contig $contig already.  Ignoring previous occurrence.\n";
	    }

	} elsif ($seglength > 0) {
	    $seglength{$contig} = $seglength;
	    if ($seglength > $opt_l) {
		if ($shortp == 1) {
		    print STDOUT "\n";
		    $shortp = 0;
		}
		if ($chronlyp == 1) {
		    printf STDOUT ("  Contig %s: concatenated at chromosome end: ",$contig);
		} else {
		    printf STDOUT ("  Contig %s: parsed chromosomal coordinates: ",$contig);
		}

		if (defined($chr_newname{$chr})) {
		    printf STDOUT ("%s:%d..",$chr_newname{$chr},$chrpos{$chr});
		} else {
		    printf STDOUT ("%s:%d..",$chr,$chrpos{$chr});
		}
		printf STDOUT ("%d (length = %d nt)",$chrpos{$chr}+$seglength-1,$seglength);
		if (defined($opt_R)) {
		    printf STDOUT (" (revcomp => %s:%d..%d)",$chr,$chrpos{$chr}+$seglength-1,$chrpos{$chr});
		} elsif (defined($orientation) && $orientation eq "rev") {
		    printf STDOUT (" (revcomp => %s:%d..%d)",$chr,$chrpos{$chr}+$seglength-1,$chrpos{$chr});
		}
		print STDOUT "\n";
		$shortp = 0;
	    } else {
		if ($shortp == 0) {
		    printf STDOUT "  Processed short contigs (<$opt_l nt): ";
		    $shortp = 1;
		}
		if (++$ncontigs < 100) {
		    printf STDOUT ".";
		} elsif ($ncontigs == 100) {
		    printf STDOUT "More than 100 short contigs.  Will stop printing.\n";
		}
	    }

	    if (defined($chr_newname{$chr})) {
		$string = sprintf("%s\t%s:",$contig,$chr_newname{$chr});
	    } else {
		$string = sprintf("%s\t%s:",$contig,$chr);
	    }
	    if (defined($opt_R)) {
		$string .= sprintf("%d..%d",$chrpos{$chr}+$seglength-1,$chrpos{$chr});
	    } elsif (defined($orientation) && $orientation eq "rev") {
		$string .= sprintf("%d..%d",$chrpos{$chr}+$seglength-1,$chrpos{$chr});
	    } else {
		$string .= sprintf("%d..%d",$chrpos{$chr},$chrpos{$chr}+$seglength-1);
	    }

	    # Note: using new name for circular argument
	    if (defined($circularp{$contig})) {
		$string .= "\tcircular";
	    } elsif (defined($altscaffold_info{$chr})) {
		$string .= "\t" . $altscaffold_info{$chr};
	    } else {
		$string .= "\tlinear";
	    }
	    $string .= "\n";

	    push @output,$string;
	    if (!defined($lowest{$chr})) {
		$lowest{$chr} = $chrpos{$chr};
	    } elsif ($chrpos{$chr} < $lowest{$chr}) {
		$lowest{$chr} = $chrpos{$chr};
	    }
	    $chrpos{$chr} += $seglength; # Used only when a header doesn't have a chrpos for this chr
	}

	undef $contig;
	close($FP);
	handle_consecutive_errors($consec_errors);
    }

    return (\@output,\@skipped);
}


sub parse_fq_files {
    my ($argv) = @_;
    my ($FP, $line, $strain);
    my @output = ();
    my @skipped = ();
    my $seglength;
    my $contig = "";
    my $nwarnings_dup = 0;
    my $nwarnings_colon = 0;
    my $ncontigs = 0;

    foreach $arg (@ {$argv}) {
	if (defined($opt_E)) {
	    printf STDOUT "Executing command $arg\n";
	    $FP = new IO::File("$arg |") or die "Can't execute $arg";
	} elsif (defined($opt_g)) {
	    printf STDOUT "Opening gzipped file $arg\n";
	    $FP = new IO::File("gunzip -c \"$arg\" |") or die "Can't execute $arg";
	} else {
	    printf STDOUT "Opening file $arg\n";
	    $FP = new IO::File("$arg") or die "Can't open file $arg";
	}

	$consec_errors = 0;
	while (defined($header = <$FP>) && defined($line = <$FP>)) {
	    $line =~ s/\r\n/\n/;
	    chomp $line;
	    if ($header =~ /^\+/) {
		# Skip quality string
	    } elsif ($header !~ /^@(\S+)/) {
		die "Expected FASTQ line to start with @";
	    } else {
		$chr = $contig = $1;
		$lowest{$chr} = $chrpos{$chr} = 1;
		$seglength = length($line);

		if (defined($seglength{$contig})) {
		    if ($seglength != $seglength{$contig}) {
			die "Saw contig $contig already, and second length $seglength is different from first $seglength{$contig}";
		    } elsif ($nwarnings_dup < 50) {
			print STDOUT "Saw contig $contig already.  Ignoring previous occurrence.\n";
		    } elsif ($nwarnings_dup == 50) {
			print STDERR "More than 50 warnings.  Will stop printing warnings\n";
		    }
		    $nwarnings_dup++;

		} else {
		    $seglength{$contig} = $seglength;

		    if (defined($chr_newname{$chr})) {
			$string = sprintf("%s\t%s:",$contig,$chr_newname{$chr});
		    } else {
			$string = sprintf("%s\t%s:",$contig,$chr);
		    }
		    if (defined($opt_R)) {
			$string .= sprintf("%d..%d",$chrpos{$chr}+$seglength-1,$chrpos{$chr});
		    } elsif (defined($orientation) && $orientation eq "rev") {
			$string .= sprintf("%d..%d",$chrpos{$chr}+$seglength-1,$chrpos{$chr});
		    } else {
			$string .= sprintf("%d..%d",$chrpos{$chr},$chrpos{$chr}+$seglength-1);
		    }
		    $string .= "\tlinear";
		    $string .= "\n";

		    push @output,$string;
		}
	    }
	}
	close($FP);
	handle_consecutive_errors($consec_errors);
    }

    return (\@output,\@skipped);
}


sub by_numeric_alpha {
    my ($numeric_a, $alpha_a, $numeric_b, $alpha_b);

    if ($a =~ /^chr\d/) {
	($numeric_a, $alpha_a) = $a =~ /chr(\d+)(\S*)/;
    } elsif ($a =~ /^chr\S/) {
	($alpha_a) = $a =~ /chr(\d+)(\S*)/;
    } elsif ($a =~ /^\d/) {
	($numeric_a, $alpha_a) = $a =~ /(\d+)(\S*)/;
    } else {
	$alpha_a = $a;
    }
    if (defined($alpha_a) && $alpha_a !~ /\S/) {
	undef $alpha_a;
    }

    if ($b =~ /^chr\d/) {
	($numeric_b, $alpha_b) = $b =~ /chr(\d+)(\S*)/;
    } elsif ($b =~ /^chr\S/) {
	($alpha_b) = $b =~ /chr(\d+)(\S*)/;
    } elsif ($b =~ /^\d/) {
	($numeric_b, $alpha_b) = $b =~ /(\d+)(\S*)/;
    } else {
	$alpha_b = $b;
    }
    if (defined($alpha_b) && $alpha_b !~ /\S/) {
	undef $alpha_b;
    }


    if (defined($numeric_a) && !defined($numeric_b)) {
	return -1;
    } elsif (defined($numeric_b) && !defined($numeric_a)) {
	return +1;
    } elsif (defined($numeric_a) && defined($numeric_b)) {
	if ($numeric_a < $numeric_b) {
	    return -1;
	} elsif ($numeric_b < $numeric_a) {
	    return +1;
	} else {
	    if (!defined($alpha_a) && defined($alpha_b)) {
		return -1;
	    } elsif (!defined($alpha_b) && defined($alpha_a)) {
		return +1;
	    } elsif (defined($alpha_a) && defined($alpha_b)) {
		return $alpha_a cmp $alpha_b;
	    } else {
		return 0;
	    }
	}
    } else {
	return $a cmp $b;
    }
}

