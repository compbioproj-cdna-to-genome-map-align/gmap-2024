

if FULLDIST
bin_SCRIPTS = gmap_process gmap_build gmap_cat md_coords fa_coords \
              psl_splicesites psl_introns psl_genes \
              ensembl_genes \
              gtf_splicesites gtf_introns gtf_genes gtf_transcript_splicesites \
              gff3_splicesites gff3_introns gff3_genes \
              dbsnp_iit gvf_iit vcf_iit
else
bin_SCRIPTS = gmap_process gmap_build gmap_cat md_coords fa_coords \
              psl_splicesites psl_introns psl_genes \
              ensembl_genes \
              gtf_splicesites gtf_introns gtf_genes gtf_transcript_splicesites \
              gff3_splicesites gff3_introns gff3_genes \
              dbsnp_iit gvf_iit vcf_iit
endif

gmap_process: gmap_process.pl
	cp gmap_process.pl gmap_process
	chmod +x gmap_process

gmap_build: gmap_build.pl
	cp gmap_build.pl gmap_build
	chmod +x gmap_build

gmap_cat: gmap_cat.pl
	cp gmap_cat.pl gmap_cat
	chmod +x gmap_cat

md_coords: md_coords.pl
	cp md_coords.pl md_coords
	chmod +x md_coords

fa_coords: fa_coords.pl
	cp fa_coords.pl fa_coords
	chmod +x fa_coords

psl_splicesites: psl_splicesites.pl
	cp psl_splicesites.pl psl_splicesites
	chmod +x psl_splicesites

psl_genes: psl_genes.pl
	cp psl_genes.pl psl_genes
	chmod +x psl_genes

psl_introns: psl_introns.pl
	cp psl_introns.pl psl_introns
	chmod +x psl_introns

psl_splices: psl_splices.pl
	cp psl_splices.pl psl_splices
	chmod +x psl_splices

ensembl_genes: ensembl_genes.pl
	cp ensembl_genes.pl ensembl_genes
	chmod +x ensembl_genes


gtf_splicesites: gtf_splicesites.pl
	cp gtf_splicesites.pl gtf_splicesites
	chmod +x gtf_splicesites

gtf_introns: gtf_introns.pl
	cp gtf_introns.pl gtf_introns
	chmod +x gtf_introns

gtf_genes: gtf_genes.pl
	cp gtf_genes.pl gtf_genes
	chmod +x gtf_genes

gtf_transcript_splicesites: gtf_transcript_splicesites.pl
	cp gtf_transcript_splicesites.pl gtf_transcript_splicesites
	chmod +x gtf_transcript_splicesites

gff3_splicesites: gff3_splicesites.pl
	cp gff3_splicesites.pl gff3_splicesites
	chmod +x gff3_splicesites

gff3_introns: gff3_introns.pl
	cp gff3_introns.pl gff3_introns
	chmod +x gff3_introns

gff3_genes: gff3_genes.pl
	cp gff3_genes.pl gff3_genes
	chmod +x gff3_genes


dbsnp_iit: dbsnp_iit.pl
	cp dbsnp_iit.pl dbsnp_iit
	chmod +x dbsnp_iit

gvf_iit: gvf_iit.pl
	cp gvf_iit.pl gvf_iit
	chmod +x gvf_iit

vcf_iit: vcf_iit.pl
	cp vcf_iit.pl vcf_iit
	chmod +x vcf_iit

if FULLDIST
CLEANFILES = gmap_process gmap_build gmap_cat md_coords fa_coords \
             psl_splicesites psl_introns psl_genes \
             ensembl_genes \
             gtf_splicesites gtf_introns gtf_genes gtf_transcript_splicesites \
             gff3_splicesites gff3_introns gff3_genes \
             dbsnp_iit gvf_iit vcf_iit
else
CLEANFILES = gmap_process gmap_build gmap_cat md_coords fa_coords \
             psl_splicesites psl_introns psl_genes \
             ensembl_genes \
             gtf_splicesites gtf_introns gtf_genes gtf_transcript_splicesites \
             gff3_splicesites gff3_introns gff3_genes \
             dbsnp_iit gvf_iit vcf_iit
endif

