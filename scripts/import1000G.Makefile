.SECONDARY:
.PHONY: groups


# The makefile runs all the steps necessary to reshape all the data into the
# format the Encore expects. The exact tools and settings used aren't that
# important and may vary based on the needs of different projects. This
# is just a starting point for importing your own data.

# update input path, leave "%" where actual chromosome names go
SOURCEVCF = /data/1000g/chr%.vcf.gz
# skip anno for now
#SOURCEANNOVCF = /data/1000g/anno.sites.vcf.gz
# update output path
OUTDIR = /data/encore/geno/cd63af30-480c-48dc-b57c-c6577e695735/
SAVDIR = $(OUTDIR)savs/
PCADIR = $(OUTDIR)pcas/
FASTA = /data/ref/hs37d5.fa

#all: $(SAVDIR)index.txt $(PCADIR)1pct.eigenvec $(OUTDIR)kinship/kinship.kin $(OUTDIR)stats/stats.json $(OUTDIR)samples.txt groups
all: $(SAVDIR)index.txt $(PCADIR)1pct.eigenvec $(OUTDIR)kinship/kinship.kin $(OUTDIR)samples.txt groups $(SAVDIR)anno.txt

VCFSTAT = /usr/local/bin/vcf-stats
EPACTS = /usr/local/bin/epacts
BCFTOOLS = /usr/local/bin/bcftools
PLINK = /usr/local/bin/plink
SAVVY = /usr/local/bin/sav
KING = /usr/local/bin/king
SNPEFF = java -Xmx16G -jar /url/local/bin/snpEff/snpEff.jar 
BGZIP = /usr/local/bin/bgzip

dir_guard = @mkdir -p $(@D)
print-%  : ; @echo $* = $($*)

AUTOSOMES =  $(shell seq 1 22) 
CHRS = $(AUTOSOMES) X 
ANNOOUT = ${SAVDIR}chr%.anno.vcf.gz
ANNOOUTS = $(foreach chr,$(CHRS), $(subst $(PERCENT),$(chr),$(ANNOOUT)))
SAVOUT = ${SAVDIR}chr%.sav
SAVOUTS = $(foreach chr,$(CHRS), $(subst $(PERCENT),$(chr),$(SAVOUT)))
PERCENT := %
PCACOUNT = 10

$(SAVOUT): $(SOURCEVCF)
	$(dir_guard)
	$(SAVVY) import --block-size 4096 -19 $^ /dev/stdout | $(SAVVY) export -e "FILTER=='PASS';VT!='SV'" -f sav /dev/stdin $@

${SAVDIR}index.txt: $(SAVOUTS) $(addsuffix .s1r, ${SAVOUTS})
	$(dir_guard)
	ls -l $^ > $@

%.s1r: %
	${SAVVY} index $^

$(ANNOOUT): $(SOURCEVCF)
	$(dir_guard)
	zcat $^ | cut -f1-9 | $(SNPEFF) ann -noStats -noLog GRCh37.75 | $(BGZIP) > $@

${SAVDIR}anno.txt: $(addsuffix .tbi, $(ANNOOUTS)) 
	$(dir_guard)
	ls -l $(basename $^) > $@

%.gz.tbi: %.gz
	tabix $^

#KINSHIP Matrix
${OUTDIR}kinship/kinship.kin: $(subst $(PERCENT),1,$(SAVOUT)) ${SAVDIR}index.txt
	$(dir_guard)
	$(EPACTS) make-kin --ref $(FASTA) --vcf $< --min-maf 0.01 --min-callrate 0.95 \
      --sepchr --out $@  --unit 5000000 --run 12

${OUTDIR}stats/stats.json: $(VCFOUTS) 
	$(dir_guard)
	$(VCFSTAT) -t 12 $^ > $@

define group_tmpl
${OUTDIR}groups/$1.chr%.grp: $$(ANNOOUT) #${SAVDIR}index.txt
	$$(dir_guard)
	$$(EPACTS) make-group --vcf $$< --format ANN --$1 --out $$@

${OUTDIR}groups/$1.grp: $$(foreach chr,$$(CHRS), ${OUTDIR}groups/$1.chr$$(chr).grp)
	$$(dir_guard)
	cat $$^ > $$@
endef

$(foreach group,nonsyn lof,$(eval $(call group_tmpl,$(group))))

groups: ${OUTDIR}groups/nonsyn.grp 

$(OUTDIR)samples.txt: $(word 1, $(SAVOUTS))
	$(SAVVY) head -i $^ > $@

$(PCADIR)1pct.chr%.bed: $(SAVOUT)
	$(dir_guard)
	$(SAVVY) export $^ | $(BCFTOOLS) view -q 0.01:minor | $(BCFTOOLS) norm -m- > $@.sav.tmp
	$(PLINK) --vcf $@.sav.tmp --double-id --keep-allele-order --maf 0.01 --out $(basename $@) --make-bed
	rm $@.sav.tmp

%.bim: %.bed
	@true
%.fam: %.bed
	@true

%.bimx: %.bim
	$(dir_guard)
	perl -lane '$$F[1] = join(":",@F[0,3,4,5]); print join("\t", @F)' $^ > $@

#Find list of LD-pruned sites
%.pruneinfo.prune.in: %.bed %.bimx
	$(dir_guard)
	$(PLINK) --bfile $(basename $<) --bim $(word 2, $^) --indep-pairwise 100 10 0.1 --out $(basename $(basename $@))

#Subset to LD pruned sites
%.pruned.bed: %.bed %.bimx %.pruneinfo.prune.in
	$(dir_guard)
	$(PLINK) --bfile $(basename $<) --bim $(word 2, $^) --extract $(word 3, $^) --out $(basename $@) --make-bed

%.merged.bed: $(foreach chr, $(AUTOSOMES), %.chr$(chr).pruned.bed) 
	$(dir_guard)
	perl -e 'print join("\n", @ARGV),"\n"' $(wordlist 2, 1000, $(basename $^)) > $*.mergelist.txt
	$(PLINK) --bfile $(basename $<) --merge-list $*.mergelist.txt --out $(basename $@) --make-bed

%.mergedthin.bed: %.merged.bed
	$(PLINK) --bfile $(basename $<) --thin .30 --out $(basename $@) --make-bed

$(PCADIR)king-unrelated.txt: $(PCADIR)1pct.mergedthin.bed
	$(KING) -b $^ --unrelated --degree 2 --cpus 32 --prefix $(subst unrelated.txt,,$@) 

$(PCADIR)king-related.txt: $(PCADIR)king-unrelated.txt
	mv $(subst .txt,_toberemoved.txt,$^) $@	

$(PCADIR)related.clst: $(PCADIR)king-unrelated.txt $(PCADIR)king-related.txt
	awk '{parts = split(FILENAME,a,/[-.]/); print $$1, $$1, a[parts-1]}' $^ > $@

%.eigenvec: %.mergedthin.bed $(PCADIR)related.clst
	$(dir_guard)
	$(PLINK) --bfile $(basename $<) --pca $(PCACOUNT) --within $(word 2, $^) --pca-cluster-names unrelated --out $(basename $@)

