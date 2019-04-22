options(stringsAsFactors=F, digits=3)

get_options <- function() {
	require(optparse)
	option_list <- list(
	  make_option("--savFile", type="character", default="",
		help="path to savvy file"),
	  make_option("--savFileIndex", type="character", default="",
		help="path to savvy index file. Indexed by tabix"),
      make_option("--sepchr", action="store_true", default=FALSE,
        help="set if chromosomes are located in different input files"),
	  make_option("--chrom", type="character",default="0",
		help="chromosome in file to be tested. If not specified, all markers in the file will be tested"),
	  make_option("--start", type="integer", default=0L,
		help="start genome position in the file to be tested"),
	  make_option("--end", type="integer",default=0L,
		help="end genome position in the file to be tested. If not specified, the whole genome will be tested"),
	  make_option("--minMAF", type="numeric", default=0,
		help="minimum minor allele frequency for markers to be tested"),
	  make_option("--minMAC", type="numeric", default=0,
		help="minimum minor allele count for markers to be tested. Note final threshold will be the greater one between minMAF and minMAC"),
	  make_option("--sampleFile", type="character", default="",
		help="File contains one column for IDs of samples in the dosage file"),
	  make_option("--GMMATmodelFile", type="character", default="",
		help="path to the input file containing the glmm model"),
	  make_option("--varianceRatioFile", type="character", default="",
		help="path to the input file containing the variance ratio"),
	  make_option("--SAIGEOutputFile", type="character", default="",
		help="path to the output file containing the SAIGE test results"),
	  make_option("--invNormalize", type="logical", default=FALSE,
		help="inverse normalize [default='FALSE']"),
	  make_option("--numLinesOutput", type="numeric", default=10000,
		help="output results for every n markers [default=10000]"),
	  make_option("--IsOutputAFinCaseCtrl", type="logical", default=FALSE,
		help="whether to output allele frequency in cases and controls for dichotomous traits [default=FALSE]"),
	  make_option("--libPath", type="character", default="",
		help="path to SAIGE R library (if not in default R libPaths)")
	)

	parser <- OptionParser(usage="%prog [options]", option_list=option_list)
	raw_args <- commandArgs(trailingOnly = TRUE)
	raw_args <- raw_args[!grepl("=$", raw_args)]
	args <- parse_args(parser, args=raw_args, positional_arguments = 0)
	opt <- args$options
	opt
}

split_on_comma <- function(x) strsplit(x,",")[[1]]

run_analysis <- function(opt) {
	if (opt$libPath != "") {
		.libPaths(opt$libPath)
	}
	library(SAIGE)

	if (opt$sepchr) {
		stopifnot(opt$chrom!="")
		target = gsub("[0-9xyXY]+$","1",opt$chrom)
		if (all(grepl(paste0("\\b", target, "\\b"), opt$savFile))) {
			opt$savFile = gsub(paste0("\\b", target, "\\b"), opt$chrom, opt$savFile)
			opt$savFileIndex = gsub(paste0("\\b", target, "\\b"), opt$chrom, opt$savFileIndex)
		} else {
			stop(paste("Cannot find", target, "to replace with", opt$chrom, 
				"in", opt$savFile, "- cannot use sepchr"))
		}
	}
	if (opt$savFileIndex == "") {
		opt$savFileIndex = paste0(opt$savFile, ".s1r")
	}

	cmd <- substitute(SPAGMMATtest(savFile = savFile,
			savFileIndex = savFileIndex,
			chrom = chrom,
			start = start,
			end = end,
			sampleFile = sampleFile,
			GMMATmodelFile = GMMATmodelFile,
			varianceRatioFile = varianceRatioFile,
			SAIGEOutputFile = SAIGEOutputFile,
			minMAF = minMAF,
			minMAC = minMAC,
			numLinesOutput = numLinesOutput,
			IsOutputAFinCaseCtrl = IsOutputAFinCaseCtrl), opt)
	print(cmd)
	eval(cmd)
}

if (!interactive()) {
	run_analysis(get_options())
}
