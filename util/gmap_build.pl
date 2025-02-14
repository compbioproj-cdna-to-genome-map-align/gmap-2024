#! /usr/bin/perl
# $Id: gmap_build.pl.in 226315 2023-02-28 18:22:20Z twu $

use warnings;	

my $package_version = "2024-02-22";

use IO::File;
use File::Copy;	
#use File::Basename;
use Getopt::Long;


Getopt::Long::Configure(qw(no_auto_abbrev no_ignore_case_always));

# Default values
my $bindir = "/Users/work/Documents/SD_Projects/gmap-2024/bin";   # dirname(__FILE__)
my $sampling = 3;
my $sleeptime = 2;

GetOptions(
    'sarray=s' => \$build_sarray_p, # build suffix array

    'B=s' => \$bindir,		# binary directory

    'D|dir=s' => \$user_gmapdb, # destination directory
    'd|genomedb=s' => \$genomename,		  # genome name
    'c|transcriptomedb=s' => \$transcriptomename, # transcriptome name
    'T|transcripts=s' => \$transcript_fasta, # transcript FASTA file
    'gff3=s' => \$transcript_gff3,  # transcript GFF3 file
    'gtf=s' => \$transcript_gtf,  # transcript GTF file

    't|nthreads=s' => \$nthreads,   # number of threads for GMAP alignment
    'G|genes=s' => \$transcript_genes, # transcript GENES file

    'n|names=s' => \$contig_name_file,   # substitute contig names
    'L|limit-to-names' => \$limit_to_names_p, # limit to names in -n file

    'M|mdfile=s' => \$mdfile,	# NCBI MD file
    'C|contigs-are-mapped' => \$contigs_mapped_p, # Each contig contains a chromosome tag in the FASTA header

    'local=s' => \$build_localdb_p, # Whether to build localdb
    'k|kmer=s' => \$kmersize, # k-mer size for genomic index (allowed: 16 or less)
    'q=s' => \$sampling,	   # sampling interval for genome (default: 3)

    's|sort=s' => \$sorting,	# Sorting
    'g|gunzip' => \$gunzipp,	# gunzip files
    'E|fasta-pipe=s' => \$fasta_pipe,		  # Pipe for providing FASTA files
    'Q|fastq' => \$fastqp, # fastq files
    'R|revcomp' => \$revcompp, # reverse complement all reads

    'w=s' => \$sleeptime, # waits (sleeps) this many seconds between steps.  Useful if there is a delay in the filesystem.

    'o|circular=s' => \$circular,    # Circular chromosomes
    '2|altscaffold=s' => \$altscaffold,  # File with altscaffold info

    'e|nmessages=s' => \$nmessages,  # Max number of warnings or messages to print
    );


if (!defined($kmersize)) {
    print STDERR "-k flag not specified, so building main hash table with default 15-mers\n";
    $kmersize = 15;
}

if (!defined($build_localdb_p)) {
    print STDERR "--local flag not specified, so building localdb by default\n";
    $build_localdb_p = 1;
}


if (!defined($genomename)) {
    print_usage();
    die "Must specify genome database name with -d flag.";

} elsif ($genomename =~ /(\S+)\/(\S+)/) {
    # No longer allowed for the -d flag since it does not allow for a transcriptome at the same level
    $d = $1;
    if (defined($destdir) && $destdir =~ /\S/) {
	$destdir = $destdir . "/" . $d;
    } else {
	$destdir = $d;
    }
    $genomename = $2;
    $genomename =~ s/\/$//;	# In case user gives -d argument with trailing slash

    print STDERR "Please do not use a full path for the --genomedb flag.\n";
    print STDERR "This was allowed in previous versions of gmap_build, but\n";
    print STDERR "this is confusing with transcriptome-guided alignment\n";
    print STDERR "You should specify --dir and --genomedb separately as\n";
    print STDERR "  --dir $destdir\n";
    print STDERR "  --genomedb $genomename\n";
    die;
}


if (defined($user_gmapdb)) {
    print STDERR "Writing files under $user_gmapdb\n";
    $gmapdb = $user_gmapdb;
} else {
    $gmapdb = "/Users/work/Documents/SD_Projects/gmap-2024/share";
    print STDERR "Destination directory not defined with -D flag, so writing files under $gmapdb\n";
}

if (defined($contig_name_file)) {
    if (defined($sorting) && $sorting eq "names") {
	# Expecting 2-column format
	$contig_hash_file = $contig_name_file;
    } else {
	# Need to look at the first line
	$FP = new IO::File("$contig_name_file") or die;
	if (defined($line = <$FP>)) {
	    chop $line;
	    if ($line =~ /(\S+)\s+(\S+)/) {
		# 2-column format
		$contig_hash_file = $contig_name_file;
	    } else {
		# 1-column format
		$contig_order_file = $contig_name_file;
	    }
	}
	close($FP);
    }
}

if (defined($limit_to_names_p)) {
    $limit_to_names_flag = "-L";
} else {
    $limit_to_names_flag = "";
}    

if (defined($sorting)) {
    $chr_order_flag = "-s $sorting";
} else {
    # Default is not to order genomes
    print STDERR "Not sorting chromosomes.  To sort chromosomes other ways, use the -s flag.\n";
    $chr_order_flag = "-s none";
}

if (!defined($gunzipp)) {
    $gunzip_flag = "";
} elsif (defined($fasta_pipe)) {
    die "Cannot use the -E (--fasta-pipe) flag with the -g (--gunzip) flag";
} else {
    $gunzip_flag = "-g";
}

if (!defined($fastqp)) {
    $fastq_flag = "";
} else {
    $fastq_flag = "-Q";
}

if (!defined($revcompp)) {
    $revcomp_flag = "";
} else {
    $revcomp_flag = "-R";
}

if (defined($circular)) {
    $circular_flag = "-c $circular";
} else {
    $circular_flag = "";
}

if (defined($altscaffold)) {
    $altscaffold_flag = "-2 $altscaffold";
} else {
    $altscaffold_flag = "";
}

if (defined($nmessages)) {
    $nmessages_flag = "-e $nmessages";
} else {
    $nmessages_flag = "";
}

if (!defined($build_sarray_p)) {
    $sarrayp = 0;		# default is to not build the suffix array
} elsif ($build_sarray_p eq "0") {
    $sarrayp = 0;
} elsif ($build_sarray_p eq "1") {
    $sarrayp = 1;
} else {
    die "Argument to --sarray needs to be 0 or 1";
}

if (defined($contigs_mapped_p)) {
    $contigs_mapped_flag = "-C";
} else {
    $contigs_mapped_flag = "";
}


if (!defined($nthreads)) {
    $nthreads = 8;
}

if ($#ARGV < 0) {
    # No genome fasta provided, so check if it exists
    if (-d "$gmapdb/$genomename") {
	$genome_exists_p = 1;
    } else {
	die "Genome FASTA files were not given, but no directory $gmapdb/$genomename was found";
    }
} else {
    # Genome fasta provided so assume that we need to build or re-build the genome
    if (-d "$gmapdb/$genomename") {
	print STDERR "Warning: Genome FASTA files were given, so we are over-writing the index files at $gmapdb/$genomename\n";
    }
    $genome_exists_p = 0;
}    

if (defined($transcript_fasta) || defined($transcript_genes) || defined($transcript_gff3) || defined($transcript_gtf)) {
    if (!defined($transcriptomename)) {
	die "A transcript gtf, gff3, FASTA, or genes file was given, but no transcriptome name was provided with the -c flag";
    }
}


# Call to create genome.transcripts directory
$dbdir = create_db($gmapdb,$genomename,$transcriptomename);



#####################################################################################

check_compiler_assumptions();

if (!defined($transcriptomename) || $genome_exists_p == 0) {
    # 1
    $genomecompfile = "$dbdir/$genomename.genomecomp";
    my $coordsfile = "$gmapdb/$genomename.coords";
    my $fasta_sources = "$gmapdb/$genomename.sources";

    $FP = new IO::File(">$fasta_sources") or die "Could not create $fasta_sources";
    foreach $fasta (@ARGV) {
	print $FP $fasta . "\n";
    }
    close($FP);


    # 2
    create_genome_version($dbdir,$genomename);

    create_coords($mdfile,$fastq_flag,$fasta_pipe,$gunzip_flag,$circular_flag,$altscaffold_flag,$contigs_mapped_flag,
		  $contig_hash_file,$contig_order_file,$bindir,$coordsfile,$fasta_sources);
    if (!(-s "$coordsfile")) {
	die "ERROR: $coordsfile not found";
    } else {
	$gmap_process_pipe = make_gmap_process_pipe($fastq_flag,$fasta_pipe,$gunzip_flag,$bindir,$coordsfile,$fasta_sources,
						    $contig_hash_file,$contig_order_file);
    }

    make_contig($nmessages_flag,$chr_order_flag,
		$bindir,$dbdir,$genomename,$gmap_process_pipe);

    compress_genome($nmessages_flag,$bindir,$dbdir,$genomename,$gmap_process_pipe);

    unshuffle_genome($bindir,$dbdir,$genomename,$genomecompfile);


    # 3
    # Note that for gmapindex, we use -D $dbdir and not -D $gmapdb
    $index_cmd = "\"$bindir/gmapindex\" -k $kmersize -q $sampling $nmessages_flag -D \"$dbdir\" -d $genomename";

    # large_genome_p means positions exceed 2^32 bp, so create positionsh file (determined by gmapindex)
    # huge_offsets_p means index offsets exceed 2^32 bp, so create pages file (determined here)
    ($large_genome_p, $huge_offsets_p) = count_index_offsets($index_cmd,$genomecompfile);
    if ($huge_offsets_p == 1) {
	$index_cmd .= " -H";
    }

    create_index_offsets($index_cmd,$genomecompfile);
    create_index_positions($index_cmd,$genomecompfile);


    if ($sarrayp == 1) {
	make_suffix_array($bindir,$dbdir,$genomename);
    }

    # 4
    # Note that for gmapindex, we use -D $dbdir and not -D $gmapdb
    if ($build_localdb_p == 0) {
	# Skip making a regiondb
    } elsif ($large_genome_p == 1) {
	# Skip making a regiondb
    } else {
	# Previously provided "-j $localsize" to gmapindex
	$index_cmd = "\"$bindir/gmapindex\" $nmessages_flag -D \"$dbdir\" -d $genomename";
	create_regiondb($index_cmd,$genomecompfile);
    }

    system("rm -f \"$fasta_sources\"");
    system("rm -f \"$coordsfile\"");
}


if (defined($transcriptomename)) {
    if (defined($transcript_gff3)) {
	create_transcriptome_from_gff3($gmapdb,$genomename,$transcriptomename,$transcript_gff3,$kmersize);
    } elsif (defined($transcript_gtf)) {
	create_transcriptome_from_gtf($gmapdb,$genomename,$transcriptomename,$transcript_gtf,$kmersize);
    } elsif (defined($transcript_genes)) {
	create_transcriptome_from_genes($gmapdb,$genomename,$transcriptomename,$transcript_genes,$kmersize);
    } elsif (defined($transcript_fasta)) {
	create_transcriptome_from_fasta($gmapdb,$genomename,$transcriptomename,$transcript_fasta,
					$kmersize,$nthreads);
    } else {
	print_usage();
	die "If you specify --transcriptomedb, you must specify either a transcript FASTA file with --transcripts or a genes file with --genes.  You can generate a genes file using gff3_genes or GMAP.";
    }

}

exit;


#####################################################################################

sub check_compiler_assumptions {
    if (system("\"$bindir/gmapindex\" -9") != 0) {
	print STDERR "There is a mismatch between this computer system and the one where gmapindex was compiled.  Exiting.\n";
	exit(9);
    }
}


sub create_db {
    my ($gmapdb, $genomename, $transcriptomename) = @_;

    print STDERR "Creating files in directory $gmapdb/$genomename\n";
    system("mkdir -p \"$gmapdb\"");
    system("mkdir -p \"$gmapdb/$genomename\"");
    system("mkdir -p \"$gmapdb/$genomename/$genomename.maps\"");
    system("chmod 755 \"$gmapdb/$genomename/$genomename.maps\"");
    if (defined($transcriptomename)) {
	system("mkdir -p \"$gmapdb/$genomename/$genomename.transcripts\"");
	system("chmod 755 \"$gmapdb/$genomename/$genomename.transcripts\"");
    }

    return "$gmapdb/$genomename";
}


sub create_genome_version {
    my ($dbdir, $genomename) = @_;

    open GENOMEVERSIONFILE, ">$dbdir/$genomename.version" or die $!;
    print GENOMEVERSIONFILE "$genomename\n";
    print GENOMEVERSIONFILE "$package_version\n";

    close GENOMEVERSIONFILE or die $!;
    sleep($sleeptime);
    return;
}

sub create_coords {
    my ($mdfile, $fastq_flag, $fasta_pipe, $gunzip_flag, $circular_flag, $altscaffold_flag, $contigs_mapped_flag,
	$contig_hash_file, $contig_order_file, $bindir, $coordsfile, $fasta_sources) = @_;
    my ($cmd, $rc);

    if (defined($mdfile)) {
	# MD file cannot specify that a chromosome is circular or altscaffold
	$cmd = "\"$bindir/md_coords\" -o \"$coordsfile\" $mdfile";
    } else {
	if (defined($fasta_pipe)) {
	    $cmd = "$fasta_pipe | \"$bindir/fa_coords\" $revcomp_flag $fastq_flag $circular_flag $altscaffold_flag $contigs_mapped_flag $limit_to_names_flag -o \"$coordsfile\"";
	} else {
	    $cmd = "\"$bindir/fa_coords\" $gunzip_flag $revcomp_flag $fastq_flag $circular_flag $altscaffold_flag $contigs_mapped_flag $limit_to_names_flag -o \"$coordsfile\"";
	}
	if (defined($contig_hash_file)) {
	    $cmd .= " -n $contig_hash_file";
	}
	if (defined($contig_order_file)) {
	    $cmd .= " -N $contig_order_file";
	}
	if (!defined($fasta_pipe)) {
	    $cmd .= " -f \"$fasta_sources\"";
	}
    }
    run_now($cmd);
    sleep($sleeptime);
    return;
}

sub make_gmap_process_pipe {
    my ($fastq_flag, $fasta_pipe, $gunzip_flag, $bindir, $coordsfile, $fasta_sources,
	$contig_hash_file, $contig_order_file) = @_;
    my $pipe;

    if (defined($fasta_pipe)) {
	$pipe = "$fasta_pipe | \"$bindir/gmap_process\" $fastq_flag -c \"$coordsfile\"";
    } else {
	$pipe = "\"$bindir/gmap_process\" $fastq_flag $gunzip_flag -c \"$coordsfile\" -f \"$fasta_sources\"";
    }
    if (defined($contig_hash_file)) {
	$pipe .= " -n $contig_hash_file";
    } elsif (defined($contig_order_file)) {
	$pipe .= " -N $contig_order_file";
    }

    return $pipe;
}

sub make_contig {
    my ($nmessages_flag, $chr_order_flag,
	$bindir, $dbdir, $genomename, $gmap_process_pipe) = @_;
    my ($cmd, $rc);

    # Note that for gmapindex, we use -D $dbdir and not -D $gmapdb
    $cmd = "$gmap_process_pipe | \"$bindir/gmapindex\" $nmessages_flag -d $genomename -D \"$dbdir\" -A $chr_order_flag";
    run_now($cmd);
    sleep($sleeptime);
    return;
}

sub compress_genome {
    my ($nmessages_flag, $bindir, $dbdir, $genomename, $gmap_process_pipe) = @_;
    my ($cmd, $rc);

    # Build forward direction (.genomecomp)
    # Note that for gmapindex, we use -D $dbdir and not -D $gmapdb
    $cmd = "$gmap_process_pipe | \"$bindir/gmapindex\" $nmessages_flag -d $genomename -D \"$dbdir\" -G";
    run_now($cmd);
    sleep($sleeptime);

    if ($sarrayp == 1) {
	# Build reverse complement (.genomecomp.rev)
	$cmd = "$gmap_process_pipe | \"$bindir/gmapindex\" $nmessages_flag -d $genomename -D \"$dbdir\" -G -r";
	run_now($cmd);
	sleep($sleeptime);
    }

    return;
}

sub unshuffle_genome {
    my ($bindir, $dbdir, $genomename, $genomecompfile) = @_;
    my ($cmd, $rc);

    $cmd = "cat \"$genomecompfile\" | \"$bindir/gmapindex\" -d $genomename -D \"$dbdir\" -U";
    run_now($cmd);
    sleep($sleeptime);

    if ($sarrayp == 1) {
	# Build reverse complement
	$cmd = "cat \"$genomecompfile.rev\" | \"$bindir/gmapindex\" -d $genomename -D \"$dbdir\" -U -r";
	run_now($cmd);
	sleep($sleeptime);
    }

    return;
}

# Do not handle revcomp genomecomp.rev/genomebits128.rev files, since they are used only by suffix array methods
sub count_index_offsets {
    my ($index_cmd, $genomecompfile) = @_;
    my ($large_genome_p, $huge_offsets_p);
    my ($cmd, $noffsets, $npositions, $result);
    
    $cmd = "cat \"$genomecompfile\" | $index_cmd -N";
    print STDERR "Running $cmd\n";
    ($noffsets,$npositions) = `$cmd` =~ /(\d+) (\d+)/;
    if ($npositions <= 4294967295) {
	print STDERR "Number of positions: $npositions => normal-sized genome\n";
	$large_genome_p = 0;
    } else {
	print STDERR "Number of positions: $npositions => large genome\n";
	$large_genome_p = 1;
    }

    if ($noffsets <= 4294967295) {
	print STDERR "Number of offsets: $noffsets => pages file not required\n";
	$huge_offsets_p = 0;
    } else {
	print STDERR "Number of offsets: $noffsets => pages file required\n";
	$huge_offsets_p = 1;
    }
    sleep($sleeptime);
    return ($large_genome_p, $huge_offsets_p);
}

sub create_index_offsets {
    my ($index_cmd, $genomecompfile) = @_;
    my ($cmd, $rc);

    $cmd = "$index_cmd -O \"$genomecompfile\"";
    run_now($cmd);
    sleep($sleeptime);
    return;
}

sub create_index_positions {
    my ($index_cmd, $genomecompfile) = @_;
    my ($cmd, $rc);

    $cmd = "$index_cmd -P \"$genomecompfile\"";
    run_now($cmd);
    sleep($sleeptime);
    return;
}

sub create_regiondb {
    my ($index_cmd, $genomecompfile) = @_;
    my ($cmd, $rc);

    $cmd = "$index_cmd -Q \"$genomecompfile\"";
    run_now($cmd);
    sleep($sleeptime);
    return;
}


sub make_suffix_array {
    my ($bindir, $dbdir, $genomename) = @_;
    my ($cmd, $rc);

    # Suffix array: forward
    $cmd = "\"$bindir/gmapindex\" -D \"$dbdir\" -d $genomename -S";
    print STDERR "Running $cmd\n";
    if (($rc = system($cmd)) != 0) {
	die "$cmd failed with return code $rc";
    }
    sleep($sleeptime);

    # LCP and child arrays
    $cmd = "\"$bindir/gmapindex\" -D \"$dbdir\" -d $genomename -L";
    print STDERR "Running $cmd\n";
    if (($rc = system($cmd)) != 0) {
	die "$cmd failed with return code $rc";
    }
    sleep($sleeptime);

    # Compressed suffix array
    # $cmd = "\"$bindir/gmapindex\" -d $genomename -F \"$dbdir\" -D \"$dbdir\" -C";
    # print STDERR "Running $cmd\n";
    # if (($rc = system($cmd)) != 0) {
    # die "$cmd failed with return code $rc";
    # }
    # sleep($sleeptime);

    return;
}


sub create_transcriptome_from_fasta {
    my ($gmapdb, $genomename, $transcriptomename, $transcript_fasta,
	$kmersize, $nthreads) = @_;
    
    # Build transcriptome and place at same directory as genome
    # Note: this is a recursive call to create a "genome" for the transcriptome
    # Can use -q 1 with transcripts because they are small
    $cmd = "\"$bindir/gmap_build\" -D \"$gmapdb\" -d $transcriptomename -k $kmersize -q 1 \"$transcript_fasta\"";
    run_now($cmd);

    # Align transcripts to the genome
    $cmd = "\"$bindir/gmap\" -D \"$gmapdb\" -d $genomename -t $nthreads -n 1 --format=map_exons \"$transcript_fasta\" > \"$transcript_fasta.gmap\"";
    run_now($cmd);

    # Create IIT file for the gene alignments

    # Note: iit_store will put chromosomes in alphanumeric order,
    # which may be different from the order in chromosome_iit or
    # missing chromosomes, so GSNAP needs to compute a crosstable with
    # chromosome_iit

    $cmd = "cat \"$transcript_fasta.gmap\" | \"$bindir/iit_store\" -o \"$transcript_fasta.genes\"";
    run_now($cmd);
    
    # Add special files.  Place these in a directory for the genome.
    $cmd = "\"$bindir/trindex\" -D \"$gmapdb\" -d $genomename -c $transcriptomename \"$transcript_fasta.genes.iit\"";
    run_now($cmd);

    system("rm -f \"$transcript_fasta.genes.iit\"");
    system("rm -f \"$transcript_fasta.gmap\"");

    return;
}


sub create_transcriptome_from_genes {
    my ($gmapdb, $genomename, $transcriptomename, $transcript_genes, $kmersize) = @_;
    my ($transcript_fasta);
    
    # Convert gene structures to a FASTA file
    $transcript_fasta = "$transcript_genes.fasta";
    $cmd = "\"$bindir/get-genome\" -D \"$gmapdb\" -d $genomename --genes \"$transcript_genes\" > \"$transcript_fasta\"";
    run_now($cmd);

    # Build transcriptome and place at same directory as genome
    # Note: this is a recursive call to create a "genome" for the transcriptome
    # Can use -q 1 with transcripts because they are small
    $cmd = "\"$bindir/gmap_build\" -D \"$gmapdb\" -d $transcriptomename -k $kmersize -q 1 \"$transcript_fasta\"";
    run_now($cmd);

    # No need to align transcripts to the genome if we have a genes file

    # Create IIT file for the genes

    # Note: iit_store will put chromosomes in alphanumeric order,
    # which may be different from the order in chromosome_iit or
    # missing chromosomes, so GSNAP needs to compute a crosstable with
    # chromosome_iit

    $cmd = "cat \"$transcript_genes\" | \"$bindir/iit_store\" -o \"$transcript_fasta.genes\"";
    run_now($cmd);
    
    # Add special files.  Place these in a directory for the genome.
    $cmd = "\"$bindir/trindex\" -D \"$gmapdb\" -d $genomename -c $transcriptomename \"$transcript_fasta.genes.iit\"";
    run_now($cmd);

    system("rm -f \"$transcript_fasta.genes.iit\"");
    system("rm -f \"$transcript_fasta\"");
    # system("rm -f \"$transcript_genes\""); --Keep given input file

    return;
}


sub create_transcriptome_from_gff3 {
    my ($gmapdb, $genomename, $transcriptomename, $transcript_gff3, $kmersize) = @_;
    
    # Convert gff3 to a genes file
    my $transcript_map = "$transcriptomename.map";
    $cmd = "\"$bindir/gff3_genes\" --exclude-PAR \"$transcript_gff3\" > \"$transcript_map\"";
    run_now($cmd);

    # Build transcriptome and place at same directory as genome.  Note: this is a recursive call
    $cmd = "\"$bindir/gmap_build\" -D \"$gmapdb\" -d $genomename -c $transcriptomename -G \"$transcript_map\"";
    run_now($cmd);

    return;
}


sub create_transcriptome_from_gtf {
    my ($gmapdb, $genomename, $transcriptomename, $transcript_gtf, $kmersize) = @_;
    
    # Convert gtf to a genes file
    my $transcript_map = "$transcriptomename.map";
    $cmd = "\"$bindir/gtf_genes\" --exclude-PAR \"$transcript_gtf\" > \"$transcript_map\"";
    run_now($cmd);

    # Build transcriptome and place at same directory as genome.  Note: this is a recursive call
    $cmd = "\"$bindir/gmap_build\" -D \"$gmapdb\" -d $genomename -c $transcriptomename -G \"$transcript_map\"";
    run_now($cmd);

    return;
}

sub run_now {
    my ($cmd) = @_;

    print STDERR "Running $cmd\n";
    if (($rc = system($cmd)) != 0) {
	die "$cmd failed with return code $rc";
    }

    return;
}



sub print_usage {
  print <<TEXT1;

gmap_build: Builds a gmap database for a genome to be used by GMAP or GSNAP.
Part of GMAP package, version $package_version.

Usage: gmap_build [options...] -d <genome> [-c <transcriptome> -T <transcript_fasta>] <genome_fasta_files>

You are free to name <genome> and <transcriptome> as you wish.  You
will use the same names when performing alignments subsequently using
GMAP or GSNAP.

Note: If adding a transcriptome to an existing genome, then there is
no need to specify the genome_fasta_files.  This way you can add
transcriptome information to an existing genome database.

Options:
    -D, --dir=STRING          Destination directory for installation (defaults to gmapdb
                                directory specified at configure time)
    -d, --genomedb=STRING     Genome name (required)

    -n, --names=STRING        Substitute names for contigs, provided in a file.
        The file can have two formats:

        1.  A file with one column per line, with each line
            corresponding to a FASTA file, in the order given to
            gmap_build.  The chromosome name for each FASTA file will
            be replaced with the desired chromosome name in the file.
            Every chromosome in the FASTA must have a corresponding line
            in the file.  This is useful if you want to rename chromosomes
            with a systematic numbering pattern.

        2.  A file with two columns per line, separated by white
            space.  In each line, the original FASTA chromosome name
            should be in column 1 and the desired chromosome name
            will be in column 2.

            The meaning of file format 2 depends on whether
            --limit-to-names is specified.  If so, the genome build will
            be limited to those chromosomes in this file.  Otherwise,
            all chromosomes in the FASTA file will be included,
            but only those chromosomes in this file will be re-named, which
            provides an easy way to change just a few chromosome names.

        This file can be combined with the --sort=names option, in
        which the order of chromosomes is that given in the file.  In
        this case, every chromosome must be listed in the file, and
        for chromosome names that should not be changed, column 2 can
        be blank (or the same as column 1).  The option of a blank
        column 2 is allowed only when specifying --sort=names,
        because otherwise, the program cannot distinguish between a
        1-column and 2-column names file.

    -L, --limit-to-names      Determines whether to limit the genome build to the lines listed
                              in the --names file.  You can limit a genome build to certain
                              chromosomes with this option, plus a --names file that either
                              renames chromosomes, or lists the same names in both columns for
                              the desired chromosomes.

    -k, --kmer=INT            k-mer value for genomic index (allowed: 15 or less, default is 15)
    -q INT                    sampling interval for genomoe (allowed: 1-3, default 3)

    -s, --sort=STRING         Sort chromosomes using given method:
                                none - use chromosomes as found in FASTA file(s) (default)
                                alpha - sort chromosomes alphabetically (chr10 before chr 1)
                                numeric-alpha - chr1, chr1U, chr2, chrM, chrU, chrX, chrY
                                chrom - chr1, chr2, chrM, chrX, chrY, chr1U, chrU
                                names - sort chromosomes based on file provided to --names flag

    -g, --gunzip              Files are gzipped, so need to gunzip each file first
    -E, --fasta-pipe=STRING   Interpret argument as a command, instead of a list of FASTA files
    -Q, --fastq               Files are in FASTQ format
    -R, --revcomp             Reverse complement all contigs
    -w INT                    Wait (sleep) this many seconds after each step (default 2)

    -o, --circular=STRING     Circular chromosomes (either a list of chromosomes separated
                                by a comma, or a filename containing circular chromosomes,
                                one per line).  If you use the --names feature, then you
                                should use the substitute name of the chromosome, not the
                                original name, for this option.  (NOTE: This behavior is different
                                from previous versions, and starts with version 2020-10-20.)

    -2, --altscaffold=STRING  File with alt scaffold info, listing alternate scaffolds,
                                one per line, tab-delimited, with the following fields:
                                (1) alt_scaf_acc, (2) parent_name, (3) orientation,
                                (4) alt_scaf_start, (5) alt_scaf_stop, (6) parent_start, (7) parent_end.

    -e, --nmessages=INT       Maximum number of messages (warnings, contig reports) to report (default 50)

    --sarray=INT              Whether to build suffix array: 0=no (default), 1=yes

Options for older genome formats:
    -M, --mdflag=STRING       Use MD file from NCBI for mapping contigs to
                                chromosomal coordinates

    -C, --contigs-are-mapped  Find a chromosomal region in each FASTA header line.
                                Useful for contigs that have been mapped
                                to chromosomal coordinates.  Ignored if the --mdflag is provided.


Options for transcriptome-guided alignment:
    -c, --transcriptomedb=STRING    Transcriptome name, plus one of these four flags:

    --gtf=FILE                GTF file containing transcripts
    --gff3=FILE               GFF3 file containing transcripts
    -G, --genes=FILE          Genes file containing transcripts
    -T, --transcripts=FILE    FASTA file containing transcripts

    -t, --nthreads=INT        Number of threads for GMAP alignment of transcripts to genome
                                (default 8).  Applies if --transcripts option is given

TEXT1
  return;
}

