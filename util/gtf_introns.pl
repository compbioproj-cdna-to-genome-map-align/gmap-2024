#! /usr/bin/perl

use warnings;

use IO::File;
use Getopt::Std;
undef $opt_C;			# If provided, will keep only introns with canonical splice sites.  Requires -d flag.
undef $opt_R;			# If provided, will report only introns with non-canonical splice sites to stdout.  Requires -d flag.
undef $opt_2;			# If provided, will print dinucleotides at splice sites.  Requires -d flag.
undef $opt_D;			# Genome directory
undef $opt_d;			# Genome index
undef $opt_E;			# Use exon_number field to determine own exon ordering
getopts("D:d:CR2E");


if (defined($opt_d)) {
    if (!defined($opt_C) && !defined($opt_R) && !defined($opt_2)) {
	print STDERR "-d flag useful only with -C, -R, or -2 flags.  Ignoring -d flag\n";
	undef $opt_d;
    } else {
	if (0) {
	    $FP = new IO::File(">&STDOUT");
	} elsif (defined($opt_D)) {
	    $FP = new IO::File("| /Users/work/Documents/SD_Projects/gmap-2024/bin/get-genome -D $opt_D -d $opt_d > get-genome.out");
	} else {
	    $FP = new IO::File("| /Users/work/Documents/SD_Projects/gmap-2024/bin/get-genome -d $opt_d > get-genome.out");
	}

	@exons = ();
	$sortp = 0;
	$last_transcript_id = "";
	while (defined($line = <>)) {
	    if ($line =~ /^\#/) {
		# Skip
	    } else {
		$line =~ s/\r\n/\n/;
		push @lines,$line;
		chop $line;
		@fields = split /\t/,$line;

		if ($fields[2] eq "exon") {
		    @info = ();
		    parse_info($fields[8]);
		    $transcript_id = get_info(\@info,"transcript_id");
		    if ($transcript_id ne $last_transcript_id) {
			if ($last_transcript_id =~ /\S/) {
			    query_dinucleotides(\@exons,$chr,$strand,$FP,$sortp);
			}
			@exons = ();
			$sortp = 0;
			$last_transcript_id = $transcript_id;
			$chr = $fields[0];
			$strand = $fields[6];
		    }
		    if (defined($opt_E) && defined($exon_number = get_info_optional(\@info,"exon_number"))) {
			$exons[$exon_number-1] = "$fields[3] $fields[4]";
		    } else {
			$sortp = 1;
			push @exons,"$fields[3] $fields[4]";
		    }
		}
	    }
	}
    }

    if ($last_transcript_id =~ /\S/) {
	query_dinucleotides(\@exons,$chr,$strand,$FP,$sortp);
    }

    close($FP);

    $FP = new IO::File("get-genome.out") or die "Cannot open get-genome.out";

} else {
    if (defined($opt_C)) {
	print STDERR "-C flag requires you to specify -d flag.  Ignoring -C flag\n";
	undef $opt_C;
    }
    if (defined($opt_R)) {
	print STDERR "-R flag requires you to specify -d flag.  Ignoring -R flag\n";
	undef $opt_R;
    }
    if (defined($opt_2)) {
	print STDERR "-2 flag requires you to specify -d flag.  Ignoring -2 flag\n";
	undef $opt_2;
    }
}



@exons = ();
$sortp = 0;
$last_transcript_id = "";
while (defined($line = get_line())) {
    if ($line =~ /^\#/) {
	# Skip
    } else {
	chop $line;
	@fields = split /\t/,$line;

	if ($fields[2] eq "exon") {
	    @info = ();
	    parse_info($fields[8]);
	    $transcript_id = get_info(\@info,"transcript_id");
	    if ($transcript_id ne $last_transcript_id) {
		if ($last_transcript_id =~ /\S/) {
		    print_exons(\@exons,$gene_name,$last_transcript_id,$chr,$strand,$FP,$sortp);
		}
		@exons = ();
		$sortp = 0;
		$gene_name = get_info(\@info,"gene_id","gene_name");
		$last_transcript_id = $transcript_id;
		$chr = $fields[0];
		$strand = $fields[6];
	    }
	    
	    if (defined($opt_E) && defined($exon_number = get_info_optional(\@info,"exon_number"))) {
		$exons[$exon_number-1] = "$fields[3] $fields[4]";
	    } else {
		$sortp = 1;
		push @exons,"$fields[3] $fields[4]";
	    }
	}
    }
}

if ($last_transcript_id =~ /\S/) {
    print_exons(\@exons,$gene_name,$last_transcript_id,$chr,$strand,$FP,$sortp);
}

if (defined($opt_d)) {
    close($FP);
}

exit;


sub get_line {
    my $line;

    if (!defined($opt_d)) {
	if (!defined($line = <>)) {
	    return;
	} else {
	    return $line;
	}
    } else {
	if ($#lines < 0) {
	    return;
	} else {
	    $line = shift @lines;
	    return $line;
	}
    }
}


sub parse_info {
    my ($list) = @_;

    if ($list !~ /\S/) {
	return;
    } elsif ($list =~ /(\S+) "([^"]+)";?(.*)/) {
	push @info,"$1 $2";
	parse_info($3);
    } elsif ($list =~ /(\S+) (\S+);?(.*)/) {
	push @info,"$1 $2";
	parse_info($3);
    } else {
	die "Cannot parse $list";
    }
}



sub get_info {
    my $info = shift @_;
    my @desired_keys = @_;
    
    foreach $desired_key (@desired_keys) {
	foreach $item (@ {$info}) {
	    ($key,$value) = $item =~ /(\S+) (.+)/;
	    if ($key eq $desired_key) {
		return $value;
	    }
	}
    }

    print STDERR "Cannot find " . join(" or ",@desired_keys) . " in " . join("; ",@ {$info}) . "\n";
    return "NA";
}


sub get_info_optional {
    my $info = shift @_;
    my @desired_keys = @_;
    
    foreach $desired_key (@desired_keys) {
	foreach $item (@ {$info}) {
	    ($key,$value) = $item =~ /(\S+) (.+)/;
	    if ($key eq $desired_key) {
		return $value;
	    }
	}
    }

    return;
}


sub get_dinucleotide {
    my ($query, $FP) = @_;
    my $dinucl;
    my $line;
    my $lastline;

    while (defined($line = <$FP>) && $line !~ /^\# Query: $query\s*$/) {
	if ($line =~ /^\# End\s*$/) {
	    print STDERR "line is $line\n";
	    die "Could not find query $query";
	}
    }

    while (defined($line = <$FP>) && $line !~ /^\# End\s*$/) {
	if ($line =~ /^\# Query: /) {
	    die "Could not find query $query";
	}
	$lastline = $line;
    }

    if (!defined($line)) {
	die "File ended while looking for query $query";
    }

    ($dinucl) = $lastline =~ /(\S\S)/;
    if (!defined($dinucl) || $dinucl !~ /\S/) {
	die "Could not find dinucl in lastline $line for query $query";
    }

    return $dinucl;
}



sub ascending_cmp {
    ($starta) = $a =~ /(\d+) \d+/;
    ($startb) = $b =~ /(\d+) \d+/;
    return $starta <=> $startb;
}

sub get_intron_bounds_plus {
    my ($exons, $sortp) = @_;
    my @starts = ();
    my @ends = ();

    if ($sortp == 1) {
	foreach $exon (sort ascending_cmp (@ {$exons})) {
	    ($start,$end) = $exon =~ /(\d+) (\d+)/;
	    push @starts,$start;
	    push @ends,$end;
	}
    } else {
	foreach $exon (@ {$exons}) {
	    ($start,$end) = $exon =~ /(\d+) (\d+)/;
	    push @starts,$start;
	    push @ends,$end;
	}
    }

    shift @starts;
    pop @ends;

    return (\@starts,\@ends);
}

sub get_intron_bounds_minus {
    my ($exons, $sortp) = @_;
    my @starts = ();
    my @ends = ();

    if ($sortp == 1) {
	foreach $exon (reverse sort ascending_cmp (@ {$exons})) {
	    ($start,$end) = $exon =~ /(\d+) (\d+)/;
	    push @starts,$start;
	    push @ends,$end;
	}
    } else {
	foreach $exon (@ {$exons}) {
	    ($start,$end) = $exon =~ /(\d+) (\d+)/;
	    push @starts,$start;
	    push @ends,$end;
	}
    }

    pop @starts;
    shift @ends;

    return (\@starts,\@ends);
}


sub query_dinucleotides {
    my ($exons, $chr, $strand, $FP, $sortp) = @_;

    $nexons = $#{$exons} + 1;
    if ($strand eq "+") {
	($starts,$ends) = get_intron_bounds_plus($exons,$sortp);
	for ($i = 0; $i < $nexons - 1; $i++) {
	    $query = sprintf("%s:%u..%u",$chr,$ends[$i]+1,$ends[$i]+2);
	    print $FP $query . "\n";

	    $query = sprintf("%s:%u..%u",$chr,$starts[$i]-2,$starts[$i]-1);
	    print $FP $query . "\n";
	}

    } elsif ($strand eq "-") {
	($starts,$ends) = get_intron_bounds_minus($exons,$sortp);
	for ($i = 0; $i < $nexons - 1; $i++) {
	    $query = sprintf("%s:%u..%u",$chr,$starts[$i]-1,$starts[$i]-2);
	    print $FP $query . "\n";

	    $query = sprintf("%s:%u..%u",$chr,$ends[$i]+2,$ends[$i]+1);
	    print $FP $query . "\n";
	}
    }
    
    return;
}



sub donor_okay_p {
    my ($donor_dinucl, $acceptor_dinucl) = @_;

    if ($donor_dinucl eq "GT") {
	return 1;
    } elsif ($donor_dinucl eq "GC") {
	return 1;
    } elsif ($donor_dinucl eq "AT" && $acceptor_dinucl eq "AC") {
	return 1;
    } else {
	return 0;
    }
}

sub acceptor_okay_p {
    my ($donor_dinucl, $acceptor_dinucl) = @_;

    if ($acceptor_dinucl eq "AG") {
	return 1;
    } elsif ($donor_dinucl eq "AT" && $acceptor_dinucl eq "AC") {
	return 1;
    } else {
	return 0;
    }
}


sub print_exons {
    my ($exons, $gene_name, $transcript_id, $chr, $strand, $FP, $sortp) = @_;

    $nexons = $#{$exons} + 1;
    if ($strand eq "+") {
	($starts,$ends) = get_intron_bounds_plus($exons,$sortp);
	for ($i = 0; $i < $nexons - 1; $i++) {
	    $intron_length = $ {$starts}[$i] - $ {$ends}[$i] - 1;
	    if (!defined($opt_d)) {
		printf ">%s.%s.intron%d/%d %s:%u..%u\n",$gene_name,$transcript_id,$i+1,$nexons-1,$chr,$ {$ends}[$i],$ {$starts}[$i];
	    } else {
		$query = sprintf("%s:%u..%u",$chr,$ends[$i]+1,$ends[$i]+2);
		$donor_dinucl = get_dinucleotide($query,$FP);

		$query = sprintf("%s:%u..%u",$chr,$starts[$i]-2,$starts[$i]-1);
		$acceptor_dinucl = get_dinucleotide($query,$FP);

		if (defined($opt_C) && donor_okay_p($donor_dinucl,$acceptor_dinucl) == 0) {
		    printf STDERR "Skipping non-canonical donor $donor_dinucl, intron length %d for %s.%s.intron%d/%d on plus strand\n",
		    $intron_length,$gene_name,$transcript_id,$i+1,$nexons-1;
		} elsif (defined($opt_C) && acceptor_okay_p($donor_dinucl,$acceptor_dinucl) == 0) {
		    printf STDERR "Skipping non-canonical acceptor $acceptor_dinucl, intron length %d for %s.%s.intron%d/%d on plus strand\n",
		    $intron_length,$gene_name,$transcript_id,$i+1,$nexons-1;
		} elsif (defined($opt_R)) {
		    if (donor_okay_p($donor_dinucl,$acceptor_dinucl) == 0 || acceptor_okay_p($donor_dinucl,$acceptor_dinucl) == 0) {
			printf ">%s.%s.intron%d/%d %s:%u..%u",$gene_name,$transcript_id,$i+1,$nexons-1,$chr,$ {$ends}[$i],$ {$starts}[$i];
			print " $donor_dinucl-$acceptor_dinucl";
			print "\n";
		    }
		} else {
		    printf ">%s.%s.intron%d/%d %s:%u..%u",$gene_name,$transcript_id,$i+1,$nexons-1,$chr,$ {$ends}[$i],$ {$starts}[$i];
		    if (defined($opt_2)) {
			print " $donor_dinucl-$acceptor_dinucl";
		    }
		    print "\n";
		}
	    }
	}

    } elsif ($strand eq "-") {
	($starts,$ends) = get_intron_bounds_minus($exons,$sortp);
	for ($i = 0; $i < $nexons - 1; $i++) {
	    $intron_length = $ {$starts}[$i] - $ {$ends}[$i] - 1;
	    if (!defined($opt_d)) {
		printf ">%s.%s.intron%d/%d %s:%u..%u\n",$gene_name,$transcript_id,$i+1,$nexons-1,$chr,$ {$starts}[$i],$ {$ends}[$i];
	    } else {
		$query = sprintf("%s:%u..%u",$chr,$starts[$i]-1,$starts[$i]-2);
		$donor_dinucl = get_dinucleotide($query,$FP);

		$query = sprintf("%s:%u..%u",$chr,$ends[$i]+2,$ends[$i]+1);
		$acceptor_dinucl = get_dinucleotide($query,$FP);

		if (defined($opt_C) && donor_okay_p($donor_dinucl,$acceptor_dinucl) == 0) {
		    printf STDERR "Skipping non-canonical donor $donor_dinucl, intron length %d for %s.%s.intron%d/%d on minus strand\n",
		    $intron_length,$gene_name,$transcript_id,$i+1,$nexons-1;
		} elsif (defined($opt_C) && acceptor_okay_p($donor_dinucl,$acceptor_dinucl) == 0) {
		    printf STDERR "Skipping non-canonical acceptor $acceptor_dinucl, intron length %d for %s.%s.intron%d/%d on minus strand\n",
		    $intron_length,$gene_name,$transcript_id,$i+1,$nexons-1;
		} elsif (defined($opt_R)) {
		    if (donor_okay_p($donor_dinucl,$acceptor_dinucl) == 0 || acceptor_okay_p($donor_dinucl,$acceptor_dinucl) == 0) {
			printf ">%s.%s.intron%d/%d %s:%u..%u",$gene_name,$transcript_id,$i+1,$nexons-1,$chr,$ {$starts}[$i],$ {$ends}[$i];
			print " $donor_dinucl-$acceptor_dinucl";
			print "\n";
		    }
		} else {
		    printf ">%s.%s.intron%d/%d %s:%u..%u",$gene_name,$transcript_id,$i+1,$nexons-1,$chr,$ {$starts}[$i],$ {$ends}[$i];
		    if (defined($opt_2)) {
			print " $donor_dinucl-$acceptor_dinucl";
		    }
		    print "\n";
		}
	    }
	}
    }
    
    return;
}

