# This make file will run SAIGE pipeline

# This script uses the reference fai index file (REFFILEINDEX) to
#   create chcuks of size BINSIZE for STEP 2
# Use THREADS= to set the number of threads to use for STEP 1
# Use "make -j" to specify the number of parallel jobs for STEP 2

# Make variables can be replaced on the command line. For example
#  make -f saige.Makefile -j20 THREADS=20 RESPONSE=BMI


OUTDIR = ./
OUTNAME = results.txt.gz
LOGNAME = saige.log

PLINKFILE = plink.bed
SAVFILE = chr1.sav
SAMPLEFILE = samples.txt
REFFILE = hs38DH.fa
REFFILEINDEX = $(addsuffix .fai, $(REFFILE))
PHENOFILE = pheno.txt
PHENOFILEIDCOL = IND_ID
RESPONSE = ""
RESPONSETYPE = quantitative
INVNORM = FALSE
COVAR = ""
BINSIZE = 500000
THREADS = 8
STEP1OPT = --memoryChunk=2
STEP2OPT = --minMAF=0.001 --IsOutputAFinCaseCtrl=FALSE

RLIBPATH = /apps/saige
RSCRIPT = R_LIBS=$(RLIBPATH) /usr/bin/Rscript
APPDIR = /apps/saige/

ifeq ("$(wildcard $(REFFILEINDEX))","")
$(error Cannot read REFFILEINDEX ($(REFFILEINDEX)) in order to create chunks for step 2)
endif

#MAKE BINS CHR.START.END
CHRS = $(shell seq 1 22) X
$(foreach chr, $(CHRS), $(eval CHR$(chr)BINS = $(shell awk -v bin=$(BINSIZE) '/^(chr)?$(chr)\y/ {s=1;e=bin; while(s<$$2) {if (e>$$2) {e=$$2}; print $$1 "." s "." e; s=s+bin; e=e+bin;}}' $(REFFILEINDEX))))
BINS = $(foreach chr, $(CHRS), $(CHR$(chr)BINS))

all: $(OUTDIR)$(OUTNAME) $(OUTDIR)$(OUTNAME).tbi $(OUTDIR)$(LOGNAME).gz

print-%  : ; @echo $* = $($*)

PID = $(shell cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w ${1:-32} | head -n 1)

$(OUTDIR)step1.rda:
	$(RSCRIPT) $(APPDIR)step1_fitNULLGLMM_v29.R \
		--plinkFile=$(basename $(PLINKFILE)) \
		--phenoFile=$(PHENOFILE) \
		--phenoCol=$(RESPONSE) \
		--covarColList=$(COVAR) \
		--sampleIDColinphenoFile=$(PHENOFILEIDCOL) \
		--traitType=$(RESPONSETYPE) \
		--outputPrefix=$(basename $@) \
		--invNormalize=$(INVNORM) \
		--nThreads=$(THREADS) \
		$(STEP1OPT) &> $(OUTDIR)$(LOGNAME)

$(OUTDIR)step2.bin.%.txt: $(OUTDIR)step1.rda
	$(RSCRIPT) $(APPDIR)step2_SPATests_savvy_v24.R \
		--savFile=$(SAVFILE) \
		--sepchr \
		--chrom=$(word 1, $(subst ., ,$*)) \
		--start=$(word 2, $(subst ., ,$*)) \
		--end=$(word 3, $(subst ., ,$*)) \
		--sampleFile=$(SAMPLEFILE) \
		--GMMATmodelFile=$^ \
		--varianceRatioFile=$(addsuffix .varianceRatio.txt, $(basename $^)) \
		--SAIGEOutputFile=$@ \
		--numLinesOutput=2 \
		$(STEP2OPT) &> $@.log

STEP2FILES = $(foreach bin,$(BINS),$(OUTDIR)step2.bin.$(bin).txt)
STEP2LOGS = $(foreach bin,$(STEP2FILES),$(bin).log)
$(OUTDIR)$(OUTNAME): $(STEP2FILES)
	awk 'FNR!=1 || NR==1' $^ | tr " " "\t" | bgzip -c > $@ && rm $^

$(OUTDIR)$(OUTNAME).tbi: $(OUTDIR)$(OUTNAME)
	tabix -s1 -b2 -e2 -S1 $^

$(OUTDIR)$(LOGNAME).gz: $(OUTDIR)$(OUTNAME).tbi
	cat $(OUTDIR)$(LOGNAME) $(STEP2LOGS) | gzip -c > $@  && rm $(STEP2LOGS) && rm $(OUTDIR)$(LOGNAME)

slurm:
	echo "$(STEP2FILES)" | tr " " "\n" | awk -v 'BEGIN{i=0}  \
		{jobs[i]=$$_; i++} \
		END{for(j=0; j<i; j++) print "jobs[" j "]=\"" jobs[j] "\""; \
		print "#SBATCH --array=0-" (i-1); \
		print "makevars=\"" mf "\""; \
		print "eval \"make -f $(lastword $(MAKEFILE_LIST))\" $$jobs[$${SLURM_ARRAY_TASK_ID}] )"}' > commands.txt

