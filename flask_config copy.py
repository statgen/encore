#!/usr/bin/env python3

import os
DEFAULT_CONFIG = {
    'SERVER_NAME': 'localhost:5000',
    'JOB_DATA_FOLDER': '/Users/snehalpatil/Documents/AbecasisLab/encorejobs/',
#   SLURM_ACCOUNT': 'encore',
    'JOB_MEM_PER_CPU': 6500,
    'JOB_CPUS_PER_TASK': 56,
    'JOB_TIME': '14-0',
    'QUEUE_PARTITION': 'encore',
    'PHENO_DATA_FOLDER': '/Users/snehalpatil/Documents/AbecasisLab/encorepheno/',
    'GENO_DATA_FOLDER': '/Users/snehalpatil/Documents/AbecasisLab/encoregeno/',
    'EPACTS_BINARY': 'epacts',
    'SAVANT_SIF_FILE':'/Users/snehalpatil/Documents/AbecasisLab/SAVANTFILES/encore-analyses.sif',
    'SAVANT_BINARY':'/Users/snehalpatil/Documents/AbecasisLab/SAVANTFILES/Snakefile-encore',
     ''
    'QUEUE_JOB_BINARY': 'sbatch',
    'CANCEL_JOB_BINARY': 'scancel',
    'MANHATTAN_BINARY': 'make_manhattan_json.py',
    'QQPLOT_BINARY': 'make_qq_json.py',
    'TOPHITS_BINARY': 'make_tophits_json.py',
    'NEAREST_GENE_BED': 'data/nearest-gene.bed',
    'VCF_FILE': '',
    'MYSQL_HOST': 'localhost',
    'MYSQL_DB': 'encore',
    'MYSQL_USER': 'prsweb',
    'MYSQL_PASSWORD': 'prsweb',
    'SECRET_KEY': os.urandom(24),
    'JWT_SECRET_KEY': None,
    'GOOGLE_LOGIN_CLIENT_ID' : '843687692706-jv2uc26gncaaqckt13r62c29q0b9gau8.apps.googleusercontent.com',
    'GOOGLE_LOGIN_CLIENT_SECRET' : 'nIEqeKR4Whf_GSYITldTNvyY',
    'LD_SERVER': 'https://portaldev.sph.umich.edu/ld/',
    'HELP_EMAIL': '',
    'BUILD_REF': {
        'GRCh37': {
            'fasta': '/data/ref/hs37d5.fa',
            'nearest_gene_bed': '/data/ref/nearest-gene.GRCh37.bed'
        },
        'GRCh38': {
            'fasta': '/data/ref/hs38DH.fa',
            'nearest_gene_bed': '/data/ref/nearest-gene.GRCh38.bed'
        }
    }
}

for key in DEFAULT_CONFIG.keys():
    if os.getenv(key):
        DEFAULT_CONFIG[key] = os.environ[key]
    globals()[key] = DEFAULT_CONFIG[key]
