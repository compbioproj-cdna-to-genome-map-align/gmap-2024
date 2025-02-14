#! /usr/bin/perl

use warnings;

my $package_version = "2024-02-22";

use Getopt::Long;
Getopt::Long::Configure(qw(no_auto_abbrev no_ignore_case_always));

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


undef $gene_line;
@transcript_lines = ();
while (defined($line = <>)) {
    if ($line =~ /^\#/) {
	# Skip
    } elsif ($line !~ /\S/) {
	# Skip blank line
    } else {
	$line =~ s/\r\n/\n/;
	chop $line;
	@fields = split /\t/,$line;

	if ($fields[2] eq "gene") {
	    if (defined($gene_line)) {
		print_gene($gene_line,\@transcript_lines);
	    }
	    $gene_line = $line;
	    @transcript_lines = ();
	} else {
	    push @transcript_lines,$line;
	}
    }
}

if (defined($gene_line)) {
    print_gene($gene_line,\@transcript_lines);
}

exit;


sub gene_class {
    my ($gene_type) = @_;

    if ($gene_type eq "IG_V_gene" || $gene_type eq "IG_D_gene" ||
	$gene_type eq "IG_J_gene" || $gene_type eq "IG_C_gene") {
	return "immune";

    } elsif ($gene_type eq "TR_V_gene" || $gene_type eq "TR_D_gene" ||
	     $gene_type eq "TR_J_gene" || $gene_type eq "TR_C_gene") {
	return "immune";

    } elsif ($gene_type eq "IG_V_gene" || $gene_type eq "IG_D_gene" ||
	     $gene_type eq "IG_J_gene" || $gene_type eq "IG_C_gene") {
	return "immune"

    } elsif ($gene_type eq "TR_V_gene" || $gene_type eq "TR_D_gene" ||
	     $gene_type eq "TR_J_gene" || $gene_type eq "TR_C_gene") {
	return "immune";

    } elsif ($gene_type eq "lincRNA") {
	return "lincrna";

    } elsif ($gene_type eq "protein_coding") {
	return "transcribed";

    } elsif ($gene_type eq "processed_transcript") {
	return "transcribed";

    } elsif ($gene_type eq "polymorphic_pseudogene") {
	return "transcribed";

    } elsif ($gene_type eq "pseudogene") {
	# Depends on the transcript
	return "transcribed";

    } else {
	return "other";
    }
}


# Note: processed pseudogenes lack introns
sub transcript_class {
    my ($transcript_type) = @_;

    if ($transcript_type eq "IG_V_gene" || $transcript_type eq "IG_D_gene" ||
	$transcript_type eq "IG_J_gene" || $transcript_type eq "IG_C_gene") {
	return "immune";

    } elsif ($transcript_type eq "TR_V_gene" || $transcript_type eq "TR_D_gene" ||
	     $transcript_type eq "TR_J_gene" || $transcript_type eq "TR_C_gene") {
	return "immune";

    } elsif ($transcript_type eq "IG_V_gene" || $transcript_type eq "IG_D_gene" ||
	     $transcript_type eq "IG_J_gene" || $transcript_type eq "IG_C_gene") {
	return "immune"

    } elsif ($transcript_type eq "TR_V_gene" || $transcript_type eq "TR_D_gene" ||
	     $transcript_type eq "TR_J_gene" || $transcript_type eq "TR_C_gene") {
	return "immune";

    } elsif ($transcript_type eq "lincRNA") {
	return "lincrna";

    } elsif ($transcript_type eq "protein_coding") {
	return "transcribed";

    } elsif ($transcript_type eq "processed_transcript") {
	return "transcribed";

    } elsif ($transcript_type eq "nonsense_mediated_decay") {
	return "transcribed";

    } elsif ($transcript_type eq "polymorphic_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "processed_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "transcribed_processed_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "transcribed_unprocessed_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "translated_unprocessed_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "unitary_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "unprocessed_pseudogene") {
	return "transcribed";

    } elsif ($transcript_type eq "sense_overlapping") {
	return "transcribed";

    } elsif ($transcript_type eq "retained_intron") {
	return "transcribed";

    } else {
	return "other";
    }
}


#sub hla_p {
#    my ($fields) = @_;
#
#    my ($gene_name) = $ {$fields}[8] =~ /gene_name=([^;]+)/;
#    if (!defined($gene_name)) {
#	return 0;
#    } elsif ($gene_name !~ /^HLA-/) {
#	return 0;
#    } else {
#	return 1;
#    }
#}


sub has_tag_p {
    my ($fields, $desired_tag) = @_;
    my ($tags, $tag);

    ($tags) = $ {$fields}[8] =~ /tag=([^;]+)/;
    if (!defined($tags)) {
	return 0;
    } else {
	foreach $tag (split ",",$tags) {
	    if ($tag eq $desired_tag) {
		return 1;
	    }
	}
	return 0;
    }
}


sub print_gene {
    my ($gene_line, $transcript_lines) = @_;
    my $transcript_line;
    my @exon_lines = ();
    my $printp;

    @fields = split /\t/,$gene_line;
    ($gene_id) = $fields[8] =~ /gene_id=([^;]+)/;
    ($gene_name) = $fields[8] =~ /gene_name=([^;]+)/;
    ($gene_type) = $fields[8] =~ /gene_type=([^;]+)/;
    $gene_class = gene_class($gene_type);

    if ($gene_class eq "transcribed") {
	# Always include transcribed
	$printp = 1;

    } elsif ($gene_class eq "immune") {
	$printp = ($exclude_immune_p == 1) ? 0 : 1;

    } elsif ($gene_class eq "lincrna") {
	$printp = ($exclude_lincrna_p == 1) ? 0 : 1;

    } elsif ($gene_class eq "other") {
	# Non-transcribed
	$printp = ($exclude_nontranscribed_p == 1) ? 0 : 1;

    } else {
	die "Unexpected gene_class $gene_class";
    }

    if ($printp == 0) {
	print STDERR "Excluding gene $gene_name because of gene type $gene_type\n";
    } else {
	foreach my $line (@ {$transcript_lines}) {
	    @fields = split /\t/,$line;

	    if ($fields[2] eq "transcript") {
		if (defined($transcript_line)) {
		    print_transcript($gene_id,$gene_name,$transcript_line,\@exon_lines);
		}
		$transcript_line = $line;
		@exon_lines = ();

	    } elsif ($fields[2] eq "exon") {
		push @exon_lines,$line;
	    }

	}

	if (defined($transcript_line)) {
	    print_transcript($gene_id,$gene_name,$transcript_line,\@exon_lines);
	}
    }

    return;
}


sub print_transcript {
    my ($gene_id, $gene_name, $transcript_line, $exon_lines) = @_;
    my $printp;

    @fields = split /\t/,$transcript_line;
    ($transcript_name) = $fields[8] =~ /transcript_name=([^;]+)/;
    ($transcript_type) = $fields[8] =~ /transcript_type=([^;]+)/;
    $transcript_class = transcript_class($transcript_type);

    if ($transcript_class eq "transcribed") {
	# Always include transcribed
	$printp = 1;

    } elsif ($transcript_class eq "immune") {
	$printp = ($exclude_immune_p == 1) ? 0 : 1;

    } elsif ($transcript_class eq "lincrna") {
	$printp = ($exclude_lincrna_p == 1) ? 0 : 1;

    } elsif ($transcript_class eq "other") {
	# Non-transcribed
	$printp = ($exclude_nontranscribed_p == 1) ? 0 : 1;

    } else {
	die "Unexpected transcript_class $transcript_class";
    }
    
    if ($printp == 0) {
	print STDERR "Excluding transcript $transcript_name because of transcript type $transcript_type\n";
    } else {
	if ($exclude_readthrough_p == 1 &&
	    has_tag_p(\@fields,"readthrough_transcript") == 1) {
	    print STDERR "Excluding transcript $transcript_name because it is readthrough\n";
	    $printp = 0;
	}

	if ($exclude_PAR_p == 1 &&
	    has_tag_p(\@fields,"PAR") == 1) {
	    print STDERR "Excluding transcript $transcript_name because it is PAR\n";
	    $printp = 0;
	}
    }
    
    if ($printp == 1) {
	print_coords($gene_id,$gene_name,$transcript_line,$exon_lines);
    }

    return;
}


sub ascending_cmp {
    ($starta) = $a =~ /(\d+) \d+/;
    ($startb) = $b =~ /(\d+) \d+/;
    return $starta <=> $startb;
}

sub get_gene_bounds_plus {
    my ($exons) = @_;
    my ($start,$end);
    my @sorted;

    @sorted = sort ascending_cmp (@ {$exons});
    ($start) = $sorted[0] =~ /(\d+) \d+/;
    ($end) = $sorted[$#sorted] =~ /\d+ (\d+)/;
    return ($start,$end);
}

sub get_gene_bounds_minus {
    my ($exons) = @_;
    my ($start,$end);
    my @sorted;

    @sorted = reverse sort ascending_cmp (@ {$exons});
    ($end) = $sorted[0] =~ /\d+ (\d+)/;
    ($start) = $sorted[$#sorted] =~ /(\d+) \d+/;
    return ($start,$end);
}


sub get_exon_bounds_plus {
    my ($exon_lines) = @_;
    my @starts = ();
    my @ends = ();

    foreach $exon (sort ascending_cmp (@ {$exon_lines})) {
	($start,$end) = $exon =~ /(\d+) (\d+)/;
	push @starts,$start;
	push @ends,$end;
    }

    return (\@starts,\@ends);
}

sub get_exon_bounds_minus {
    my ($exons) = @_;
    my @starts = ();
    my @ends = ();

    foreach $exon (reverse sort ascending_cmp (@ {$exons})) {
	($start,$end) = $exon =~ /(\d+) (\d+)/;
	push @starts,$start;
	push @ends,$end;
    }

    return (\@starts,\@ends);
}


# $regions can be exons or CDS regions
sub print_coords {
    my ($gene_id, $gene_name, $transcript_line, $exon_lines) = @_;
    my @fields;
    my @exons = ();
    my ($starts, $ends);

    @fields = split /\t/,$transcript_line;
    ($transcript_id) = $fields[8] =~ /transcript_id=([^;]+)/;

    my $chr = $fields[0];
    my $strand = $fields[6];

    foreach my $line (@ {$exon_lines}) {
	@fields = split /\t/,$line;
	push @exons,"$fields[3] $fields[4]";
    }


    if ($strand eq "+") {
	($start,$end) = get_gene_bounds_plus(\@exons);
	printf ">$transcript_id $chr:%u..%u\n",$start,$end;
    } elsif ($strand eq "-") {
	($start,$end) = get_gene_bounds_minus(\@exons);
	printf ">$transcript_id $chr:%u..%u\n",$end,$start;
    } else {
	die "strand $strand";
    }

    print "$gene_name $gene_id\n";

    $nexons = $#exons + 1;
    if ($strand eq "+") {
	($starts,$ends) = get_exon_bounds_plus(\@exons);
	for ($i = 0; $i < $nexons; $i++) {
	    printf "%u %u\n",$ {$starts}[$i],$ {$ends}[$i];
	}
    } elsif ($strand eq "-") {
	($starts,$ends) = get_exon_bounds_minus(\@exons);
	for ($i = 0; $i < $nexons; $i++) {
	    printf "%u %u\n",$ {$ends}[$i],$ {$starts}[$i];
	}
    }
    
    return;
}

# gene_type in gencode can be:

#     100 3prime_overlapping_ncrna
#   41470 antisense
#     185 IG_C_gene
#      36 IG_C_pseudogene
#     152 IG_D_gene
#      79 IG_J_gene
#       9 IG_J_pseudogene
#    1119 IG_V_gene
#     681 IG_V_pseudogene
#   51893 lincRNA
#    9165 miRNA
#    6102 misc_RNA
#       6 Mt_rRNA
#      66 Mt_tRNA
#    3492 polymorphic_pseudogene
#   13567 processed_transcript
# 2398993 protein_coding
#   70989 pseudogene
#    1581 rRNA
#    3175 sense_intronic
#    1352 sense_overlapping
#    4371 snoRNA
#    5748 snRNA
#      58 TR_C_gene
#      12 TR_D_gene
#     298 TR_J_gene
#      12 TR_J_pseudogene
#     756 TR_V_gene
#      99 TR_V_pseudogene


# transcript_type in gencode can be:

#     100 3prime_overlapping_ncrna
#   44207 antisense
#     185 IG_C_gene
#      36 IG_C_pseudogene
#     152 IG_D_gene
#      79 IG_J_gene
#       9 IG_J_pseudogene
#    1119 IG_V_gene
#     681 IG_V_pseudogene
#   54584 lincRNA
#    9287 miRNA
#    6134 misc_RNA
#       6 Mt_rRNA
#      66 Mt_tRNA
#  281161 nonsense_mediated_decay
#    1066 non_stop_decay
#    1699 polymorphic_pseudogene
#   22972 processed_pseudogene
#  154780 processed_transcript
# 1847750 protein_coding
#   15239 pseudogene
#  135772 retained_intron
#    1589 rRNA
#    3148 sense_intronic
#    1417 sense_overlapping
#    4515 snoRNA
#    5762 snRNA
#    1091 transcribed_processed_pseudogene
#    7090 transcribed_unprocessed_pseudogene
#       3 translated_processed_pseudogene
#      58 TR_C_gene
#      12 TR_D_gene
#     298 TR_J_gene
#      12 TR_J_pseudogene
#     756 TR_V_gene
#      99 TR_V_pseudogene
#    1430 unitary_pseudogene
#   11202 unprocessed_pseudogene

