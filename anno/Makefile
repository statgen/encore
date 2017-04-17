.SECONDARY:

GENETYPES= protein_coding|IG_V_gene|TR_V_gene|TR_J_gene|IG_C_gene|IG_D_gene|IG_J_gene|TR_C_gene|TR_D_gene

all: nearest_gene.GRCh37.bed nearest_gene.GRCh38.bed cytoband.hg19.positions.txt

gencode.GRCh37.gtf.gz:
	wget -O $@ "ftp://ftp.sanger.ac.uk/pub/gencode/Gencode_human/release_19/gencode.v19.annotation.gtf.gz"

gencode.GRCh38.gtf.gz:
	wget -O $@ "ftp://ftp.sanger.ac.uk/pub/gencode/Gencode_human/release_26/gencode.v26.annotation.gtf.gz"

real_gene_exons.%.txt: gencode.%.gtf.gz
	zgrep -w -e "exon" $< | \
	egrep 'gene_type "($(GENETYPES))"' | \
	egrep -v 'gene_status "(NOVEL|PUTATIVE)"' | \
	perl -F"\t" -lane '($$gene) = $$F[8] =~ /gene_name "([^"]+)"/; $$F[8]=$$gene; print join("\t", @F)' | \
	cut -f1,4,5,9 | sort -k1,1V -k2n | uniq > $@
	
exon_events.%.txt:  real_gene_exons.%.txt
	awk '{print $$1, $$3, -1, $$4; print $$1, $$2, 1, $$4}' $< | sort -k1,1V -k2n -k3n > $@

nearest_gene.%.bed: exon_events.%.txt
	python make_nearest.py $^ > $@

cytoband.hg19.txt.gz:
	wget -O $@ "http://hgdownload.cse.ucsc.edu/goldenPath/hg19/database/cytoBand.txt.gz"
	
cytoband.hg19.positions.txt: cytoband.hg19.txt.gz
	zcat $^ | awk 'BEGIN {end=0; center=0; chr=""} (chr!="" && $$1!=chr) {print chr, center, end; center=0; end=0} (center==0 && $$5=="acen") {center=$$3} {chr=$$1; end=$$3} END {print chr, center, end}' > $@

cytoband.hg19.positions.json: cytoband.hg19.positions.txt
	awk '{print "{chr: \"" $$1 "\", center: " $$2 ", end: " $$3 "},"}' $^ > $@ 

