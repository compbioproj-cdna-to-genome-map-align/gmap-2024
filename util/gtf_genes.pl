#! /usr/bin/perl

use warnings;

my $package_version = "2024-02-22";

use Getopt::Long;
Getopt::Long::Configure(qw(no_auto_abbrev no_ignore_case_always));

# These options may not work for GTF, which has a more limited set of annotations than GFF3
GetOptions(
    'T|exclude-nontranscribed' => \$exclude_nontranscribed_p,
    'I|exclude-immune' => \$exclude_immune_p,
    'L|exclude-lincrna' => \$exclude_lincrna_p,
    'R|exclude-readthrough' => \$exclude_readthrough_p,
    'P|exclude-PAR' => \$exclude_PAR_p,
    'E|exons' => \$exons_p,
    'C|cds' => \$cds_p,
    );

# Protein coding or processed transcript
if (!defined($exclude_nontranscribed_p)) {
    $exclude_nontranscribed_p = 0;
}

if (!defined($exclude_immune_p)) {
    $exclude_immune_p = 0;
}

if (!defined($exclude_lincrna_p)) {
    $exclude_lincrna_p = 0;
}

if (!defined($exclude_readthrough_p)) {
    $exclude_readthrough_p = 0;
}

if (!defined($exclude_PAR_p)) {
    $exclude_PAR_p = 0;
}

if (!defined($exons_p)) {
    $exons_p = 1;
}

if (!defined($cds_p)) {
    $cds_p = 0;
}


@exons = ();
$sortp = 0;
$last_transcript_id = "";
while (defined($line = <>)) {
    if ($line =~ /^\#/) {
	# Skip
    } else {
	$line =~ s/\r\n/\n/;
	chop $line;
	@fields = split /\t/,$line;

	if ($fields[2] eq "exon") {
	    @info = ();
	    parse_info($fields[8]);
	    $transcript_id = get_info(\@info,"transcript_id");
	    if ($transcript_id ne $last_transcript_id) {
		if ($last_transcript_id =~ /\S/) {
		    if ($strand eq "+") {
			($start,$end) = get_gene_bounds_plus(\@exons,$sortp);
			printf ">$last_transcript_id $chr:%u..%u\n",$start,$end;
		    } elsif ($strand eq "-") {
			($start,$end) = get_gene_bounds_minus(\@exons,$sortp);
			printf ">$last_transcript_id $chr:%u..%u\n",$end,$start;
		    } else {
			die "strand $strand";
		    }
		    print "$gene_name\n";
		    print_exons(\@exons,$gene_name,$last_transcript_id,$chr,$strand,$sortp);
		}
		@exons = ();
		$sortp = 0;
		$gene_name = cat_info(\@info,"gene_id","gene_name");
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
    if ($strand eq "+") {
	($start,$end) = get_gene_bounds_plus(\@exons,$sortp);
	printf ">$last_transcript_id $chr:%u..%u\n",$start,$end;
    } elsif ($strand eq "-") {
	($start,$end) = get_gene_bounds_minus(\@exons,$sortp);
	printf ">$last_transcript_id $chr:%u..%u\n",$end,$start;
    } else {
	die "strand $strand";
    }
    print "$gene_name\n";
    print_exons(\@exons,$gene_name,$last_transcript_id,$chr,$strand,$sortp);
}


exit;


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

sub cat_info {
    my $info = shift @_;
    my @desired_keys = @_;
    my @result = ();
    
    foreach $desired_key (@desired_keys) {
	foreach $item (@ {$info}) {
	    ($key,$value) = $item =~ /(\S+) (.+)/;
	    if ($key eq $desired_key) {
		push @result,$value;
	    }
	}
    }

    if ($#result < 0) {
	print STDERR "Cannot find " . join(" or ",@desired_keys) . " in " . join("; ",@ {$info}) . "\n";
	return "NA";
    } else {
	return join(" ",@result);
    }
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

sub ascending_cmp {
    ($starta) = $a =~ /(\d+) \d+/;
    ($startb) = $b =~ /(\d+) \d+/;
    return $starta <=> $startb;
}

sub get_gene_bounds_plus {
    my ($exons, $sortp) = @_;
    my ($start,$end);
    my @sorted;

    if ($sortp == 1) {
	@sorted = sort ascending_cmp (@ {$exons});
    } else {
	@sorted = @ {$exons};
    }
    ($start) = $sorted[0] =~ /(\d+) \d+/;
    ($end) = $sorted[$#sorted] =~ /\d+ (\d+)/;
    return ($start,$end);
}

sub get_gene_bounds_minus {
    my ($exons, $sortp) = @_;
    my ($start,$end);
    my @sorted;

    if ($sortp == 1) {
	@sorted = reverse sort ascending_cmp (@ {$exons});
    } else {
	@sorted = @ {$exons};
    }
    ($end) = $sorted[0] =~ /\d+ (\d+)/;
    ($start) = $sorted[$#sorted] =~ /(\d+) \d+/;
    return ($start,$end);
}


sub get_exon_bounds_plus {
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

    return (\@starts,\@ends);
}

sub get_exon_bounds_minus {
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

    return (\@starts,\@ends);
}

sub print_exons {
    my ($exons, $gene_name, $transcript_id, $chr, $strand, $sortp) = @_;

    $nexons = $#{$exons} + 1;
    if ($strand eq "+") {
	($starts,$ends) = get_exon_bounds_plus($exons,$sortp);
	for ($i = 0; $i < $nexons; $i++) {
	    printf "%u %u\n",$ {$starts}[$i],$ {$ends}[$i];
	}
    } elsif ($strand eq "-") {
	($starts,$ends) = get_exon_bounds_minus($exons,$sortp);
	for ($i = 0; $i < $nexons; $i++) {
	    printf "%u %u\n",$ {$ends}[$i],$ {$starts}[$i];
	}
    }
    
    return;
}

