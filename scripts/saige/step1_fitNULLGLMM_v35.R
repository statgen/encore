options(stringsAsFactors=F)

## set list of cmd line arguments
get_options <- function() {
	require(optparse) #install.packages("optparse")
	option_list <- list(
	  make_option("--plinkFile", type="character", default="",
		help="path to plink file to be used for the kinship matrix"),
	  make_option("--phenoFile", type="character", default="",
		help="path to the phenotype file, a column 'IID' is required"),
	  make_option("--phenoCol", type="character", default="",
		help="coloumn name for phenotype in phenotype file, a column 'IID' is required"),
	  make_option("--traitType", type="character", default="binary",
		help="binary/quantitative [default=binary]"),
	  make_option("--covarColList", type="character", default="",
		help="list of covariates (comma separated)"),
	  make_option("--sampleIDColinphenoFile", type="character", default="IID",
		help="Column name of the IDs in the phenotype file"),
	  make_option("--nThreads", type="integer", default=16,
		help="Number of threads"),
	  make_option("--skipModelFitting", type="logical", default=FALSE,
		help="skip model fitting, [default='FALSE']"),
	  make_option("--traceCVcutoff", type="numeric", default=1,
		help="The threshold for coefficient of variation (CV) for the trace estimator. Number of runs for trace estimation will be increased until the CV is below the threshold. By default 1. suggested: 0.0025. This option has not been extensively tested."),
	  make_option("--ratioCVcutoff", type="numeric", default=1,
		help="The threshold for coefficient of variation (CV) for estimating the variance ratio. The number of randomly selected markers will be increased until the CV is below the threshold. By default 1. suggested 0.001. This option has not been extensively tested."),
	  make_option("--LOCO", type="logical", default=FALSE,
		help="Whether to apply the leave-one-chromosome-out (LOCO) approach. By default, FALSE. This option has not been extensively tested."), 
	  make_option("--outputPrefix", type="character", default="~/",
		help="path to the output files [default='~/']"),
	  make_option("--numMarkers", type="integer", default=30,
		help="An integer greater than 0 Number of markers to be used for estimating the variance ratio [default=30]"),
	  make_option("--invNormalize", type="logical", default=FALSE,
		help="inverse normalize [default='FALSE']"),
	  make_option("--memoryChunk", type="integer", default=4,
		help="number of memory chunks [default=4]"),
	  make_option("--libPath", type="character", default="",
		help="path to SAIGE R library (if not in default R libPaths)")
	)
	## list of options
	parser <- OptionParser(usage="%prog [options]", option_list=option_list)
	raw_args <- commandArgs(trailingOnly = TRUE)
	raw_args <- raw_args[!grepl("=$", raw_args)]
	args <- parse_args(parser, args = raw_args, positional_arguments = 0)
	opt <- args$options
	opt
}

split_on_comma <- function(x) strsplit(x,",")[[1]]

run_analysis <- function(opt) {
	if (opt$libPath != "") {
		.libPaths(opt$libPath)
	}
	library(SAIGE)
	print(sessionInfo())
	cmd <- substitute(fitNULLGLMM(plinkFile=plinkFile,
			phenoFile = phenoFile,
			phenoCol = phenoCol,
			traitType = traitType,
			invNormalize = invNormalize,
			covarColList = split_on_comma(covarColList),
			qCovarCol = NULL,
			sampleIDColinphenoFile = sampleIDColinphenoFile,
			tol = 0.02,
			maxiter = 20,
			tolPCG = 1e-5,
			maxiterPCG = 700,
			nThreads = nThreads,
			numMarkers = numMarkers,
			memoryChunk = memoryChunk,
			skipModelFitting = skipModelFitting,
			traceCVcutoff = traceCVcutoff,
			ratioCVcutoff = ratioCVcutoff,
			LOCO = LOCO,
			outputPrefix = outputPrefix), opt)
	print(cmd)
	eval(cmd)

}

if (!interactive()) {
	run_analysis(get_options())
}

