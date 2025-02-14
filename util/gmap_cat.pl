#! /usr/bin/perl
# $Id: gmap_cat.pl.in 221758 2020-02-13 21:37:39Z twu $

use warnings;	

my $gmapdb = "/Users/work/Documents/SD_Projects/gmap-2024/share";
my $package_version = "2024-02-22";
my $bindir = "/Users/work/Documents/SD_Projects/gmap-2024/bin";   # dirname(__FILE__)


use IO::File;
use Getopt::Long;
use File::Basename;

Getopt::Long::Configure(qw(no_auto_abbrev no_ignore_case_always));

GetOptions(
    'D|dir=s' => \$destdir,	# destination directory
    'd|db=s' => \$dbname,	# genome name
    'n|names=s' => \$contig_name_file,   # substitute contig names
    'local=s' => \$build_localdb_p, # Whether to build localdb
    );    

@input_genomes = @ARGV;
if (!defined($dbname)) {
    print_usage();
    die "Must specify destination genome database name with -d flag.";

} elsif ($#input_genomes < 0) {
    print_usage();
    die "Must specify one or more input genome databases on the command line.";

} elsif ($dbname =~ /(\S+)\/(\S+)/) {
    $dbdir = $1;
    $dbname = $2;
    if (defined($destdir) && $destdir =~ /\S/) {
	# Note: The -D and -F arguments to gmapindex are different from the -D argument to gmap/gsnap.
	# For gmapindex, we use -D /path/to/dir/dbname -d dbname.  For gmap/gsnap, we use -D /path/to/dir -d dbname.
	$destdir = $destdir . "/" . $dbname;
    } else {
	$destdir = $dbdir;
    }
}

if (!defined($destdir) || $destdir !~ /\S/) {
    print STDERR "Destination directory not defined with -D flag, so writing to $gmapdb\n";
    $destdir = $gmapdb;
}

if (!defined($build_localdb_p)) {
    print STDERR "--local flag not specified, so building localdb by default\n";
    $build_localdb_p = 1;
}


if (defined($contig_name_file)) {
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

if (defined($contig_hash_file)) {
    # Expecting 2-column format
    $FP = new IO::File($contig_hash_file) or die;
    while (defined($line = <$FP>)) {
	chop $line;
	($old,$new) = split /\s+/,$line;
	if (defined($old) && defined($new) && $new =~ /\S/) {
	    $contig_newname{$old} = $new;
	}
    }
    close($FP);

} elsif (defined($contig_order_file)) {
    # Expecting 1-column format
    $FP = new IO::File($contig_order_file) or die;
    @ {$contig_newnames_byorder} = ();
    while (defined($line = <$FP>)) {
	($new) = $line =~ /(\S+)/;
	push @ {$contig_newnames_byorder},$new;
    }
    close($FP);
    $contig_i = 0;
}


########################################################################

$dbdir = create_db($destdir,$dbname);
$genomecompfile = "$dbdir/$dbname.genomecomp";

check_compiler_assumptions();
create_genome_version($dbdir,$dbname);


# (1) Write chromosome file and chromosome IIT file
print STDERR "Merging chromosomes\n";
$OUT = new IO::File(">$dbdir/$dbname.chromosome");
$IIT = new IO::File(" | $bindir/iit_store -1 -o $dbdir/$dbname.chromosome");

%seenp = ();
$genomelen = 0;
foreach $input_genome (@input_genomes) {
    $input_dbroot = basename($input_genome);
    if (!defined($FP = new IO::File("$input_genome/$input_dbroot.chromosome"))) {
	print STDERR "Expecting full paths to input genomes";
	die "Cannot open $input_genome/$input_dbroot.chromosome in any of the given sourcedirs";
    }

    while (defined($line = <$FP>)) {
	chop $line;
	@fields = split /\t/,$line;
	$contig_oldname = $fields[0];
	($univcoord_start,$univcoord_end) = $fields[1] =~ /(\d+)\.\.(\d+)/;
	$chrlength = $fields[2];
	if (defined($fields[3]) && $fields[3] eq "circular") {
	    $circularp = 1;
	} else {
	    $circularp = 0;
	}

	if (defined($contig_newnames_byorder)) {
	    #print STDERR "Changing name $contig_oldname to $ {$contig_newnames_byorder}[$contig_i]\n";
	    $contig = $ {$contig_newnames_byorder}[$contig_i++];
	} elsif (defined($contig_newname{$contig_oldname})) {
	    #print STDERR "Changing name $contig_oldname to $contig_newname{$contig_oldname}\n";
	    $contig = $contig_newname{$contig_oldname};
	} else {
	    $contig = $contig_oldname;
	}
	if (defined($seenp{$contig})) {
	    die "Error: $contig is duplicated in the genomes.  To rename chromosomes, use the -n flag";
	} else {
	    $seenp{$contig} = 1;
	}
	if ($circularp == 1) {
	    printf $OUT "$contig\t%u..%u\t$chrlength\tcircular\n",$univcoord_start+$genomelen,$univcoord_end+$genomelen;
	    printf $IIT ">$contig %u %u circular\n",$univcoord_start+$genomelen-1,$univcoord_end+$genomelen-1;
	} else {
	    printf $OUT "$contig\t%u..%u\t$chrlength\n",$univcoord_start+$genomelen,$univcoord_end+$genomelen;
	    printf $IIT ">$contig %u %u\n",$univcoord_start+$genomelen-1,$univcoord_end+$genomelen-1;
	}
    }
    close($FP);

    if ($circularp == 1) {
	# If last chromosome is circular
	$genomelen = $univcoord_end+$chrlength+$genomelen;
    } else {
	$genomelen = $univcoord_end+$genomelen;
    }
}
close($IIT);
close($OUT);


# (2) Merge genomecomp and genomebits
print STDERR "Merging genome strings\n";
$cmd = "\"$bindir/gmapindex\" -D \"$dbdir\" -d \"$dbname\" -C " . join(" ",@input_genomes);
print STDERR "Running $cmd\n";
if (($rc = system($cmd)) != 0) {
    die "$cmd failed with return code $rc";
}

# TODO: Make a specific program for merging genomebits
unshuffle_genome($bindir,$dbdir,$dbname,$genomecompfile);


# (3) Merge indexdbs (offsets and positions)
print STDERR "Merging offsets and positions\n";
$cmd = "\"$bindir/indexdb_cat\" -D \"$dbdir\" -d \"$dbname\" " . join(" ",@input_genomes);
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
#    print STDERR "Number of offsets: $noffsets => pages file not required\n";
    $huge_offsets_p = 0;
} else {
#    print STDERR "Number of offsets: $noffsets => pages file required\n";
    $huge_offsets_p = 1;
}


# (4) Create localdb
if ($build_localdb_p == 0) {
    # Skip
} elsif ($large_genome_p == 1) {
    # Skip
} else {
    $index_cmd = "\"$bindir/gmapindex\" -D \"$dbdir\" -d $dbname";
    create_regiondb($index_cmd,$genomecompfile);
}    


print STDERR "\n";
print STDERR "**********************************************************************************\n";
print STDERR "*   gmap_cat is done.  New genome index should be in $dbdir\n";
print STDERR "**********************************************************************************\n";

exit;


# Note: We do not generate <genome>.chrsubset, <genome>.contig, or <genome>.contig.iit, but these are obsolete, anyway

# Taken from gmap_build
sub check_compiler_assumptions {
    if (system("\"$bindir/gmapindex\" -9") != 0) {
	print STDERR "There is a mismatch between this computer system and the one where gmapindex was compiled.  Exiting.\n";
	exit(9);
    }
}

# Taken from gmap_build
sub create_db {
    my ($destdir, $dbname) = @_;

    print STDERR "Creating files in directory $destdir/$dbname\n";
    system("mkdir -p \"$destdir\"");
    system("mkdir -p \"$destdir/$dbname\"");
    system("mkdir -p \"$destdir/$dbname/$dbname.maps\"");
    system("chmod 755 \"$destdir/$dbname/$dbname.maps\"");

    return "$destdir/$dbname";
}

# Taken from gmap_build
sub create_genome_version {
    my ($dbdir, $dbname) = @_;

    open GENOMEVERSIONFILE, ">$dbdir/$dbname.version" or die $!;
    print GENOMEVERSIONFILE "$dbname\n";
    print GENOMEVERSIONFILE "$package_version\n";

    close GENOMEVERSIONFILE or die $!;
    return;
}

# Taken from gmap_build
sub unshuffle_genome {
    my ($bindir, $dbdir, $dbname, $genomecompfile) = @_;
    my ($cmd, $rc);

    $cmd = "cat \"$genomecompfile\" | \"$bindir/gmapindex\" -d $dbname -D \"$dbdir\" -U";
    print STDERR "Running $cmd\n";
    if (($rc = system($cmd)) != 0) {
	die "$cmd failed with return code $rc";
    }
    return;
}

sub create_regiondb {
    my ($index_cmd, $genomecompfile) = @_;
    my ($cmd, $rc);

    $cmd = "$index_cmd -Q \"$genomecompfile\"";
    print STDERR "Running $cmd\n";
    if (($rc = system($cmd)) != 0) {
	die "$cmd failed with return code $rc";
    }
    return;
}


sub print_usage {
  print <<TEXT1;

gmap_cat: Concatenates gmap databases to be used by GMAP or GSNAP.
Part of GMAP package, version $package_version.

Usage: gmap_cat [options...] -d <output_genome_name> <path/to/genome_dir...>

Note: Input genomes should contain a full or local path for the directory containing the genome index

Options:
    -D, --dir=STRING          Destination directory for output genome index
    -d, --db=STRING           Output genome name

    -n, --names=STRING        Substitute names for renaming contigs, provided in a file.  The file have two formats:

                                1.  A file with one column per line, with each line corresponding to a FASTA file, in the order
                                    of the input genomes.  The chromosome name for each FASTA file will be replaced with the
                                    desired chromosome name in the file.  Every chromosome must have a corresponding line in the file.

                                2.  A file with two columns per line, separated by white space.  In each line, the original
                                    FASTA chromosome name should be in column 1 and the desired chromosome name will be
                                    in column 2.  Not every chromosome needs to be listed, which provides an easy way to change
                                    a few chromosome names.

TEXT1
  return;
}
