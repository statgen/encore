#!/usr/bin/env python2

'''
This script takes two arguments:
- an input filename (the output of epacts with a single phenotype)
- an output filename (please end it with `.json`)

It creates a json file which can be used to render a QQ plot.
'''

from __future__ import print_function, division, absolute_import

import os.path
import sys
import gzip
import re
import json
import math
import collections
# import scipy.stats

data_dir = '/var/pheweb_data/'
NEGLOG10_PVAL_BIN_SIZE = 0.05 # Use 0.05, 0.1, 0.15, etc
NEGLOG10_PVAL_BIN_DIGITS = 2 # Then keep 2 digits after the decimal
NUM_BINS = 1000

NUM_MAF_RANGES = 4 # Split MAF into 4 equally-sized ranges


def round_sig(x, digits):
    return 0 if x==0 else round(x, digits-1-int(math.floor(math.log10(abs(x)))))
assert round_sig(0.00123, 2) == 0.0012
assert round_sig(1.59e-10, 2) == 1.6e-10


def parse_marker_id(marker_id):
    try:
        chr1, pos1, ref, alt, opt_info= re.match(r'([^:]+):([0-9]+)_([-ATCG]+)/([-ATCG]+)(?:_(.+))?', marker_id).groups()
        #assert chr1 == chr2
        #assert pos1 == pos2
    except:
        print(marker_id)
        raise
    return chr1, int(pos1), ref, alt


def approx_equal(a, b, tolerance=1e-4):
    return abs(a-b) <= max(abs(a), abs(b)) * tolerance
assert approx_equal(42, 42.0000001)
assert not approx_equal(42, 42.01)


# def gc_value(median_pval):
#     # This should be equivalent to this R: `qchisq(p, df=1, lower.tail=F) / qchisq(.5, df=1, lower.tail=F)`
#     return scipy.stats.chi2.ppf(1 - median_pval, 1) / scipy.stats.chi2.ppf(0.5, 1)
# assert approx_equal(gc_value(0.49), 1.047457) # I computed these using that R code.
# assert approx_equal(gc_value(0.5), 1)
# assert approx_equal(gc_value(0.50001), 0.9999533)
# assert approx_equal(gc_value(0.6123), 0.5645607)


Variant = collections.namedtuple('Variant', ['neglog10_pval', 'maf'])
def parse_variant_line(variant_line, column_names):
    v = variant_line.split('\t')
    #assert v[1] == v[2]
    if v[column_names.index("PVALUE")] == 'NA' or v[column_names.index("BETA")] == 'NA':
        assert v[column_names.index("PVALUE")] == 'NA' and v[column_names.index("BETA")] == 'NA'
    else:
        chrom, pos, maf, pval, beta, sebeta = v[column_names.index("#CHROM")], int(v[column_names.index("BEGIN")]), float(v[column_names.index("MAF")]), float(v[column_names.index("PVALUE")]), float(v[column_names.index("BETA")]), float(v[column_names.index("SEBETA")])
        chrom2, pos2, ref, alt = parse_marker_id(v[column_names.index("MARKER_ID")])
        assert chrom == chrom2
        assert pos == pos2
        return Variant(-math.log10(pval), maf)


def rounded(x):
    return round(x // NEGLOG10_PVAL_BIN_SIZE * NEGLOG10_PVAL_BIN_SIZE, NEGLOG10_PVAL_BIN_DIGITS)

Variant = collections.namedtuple('Variant', ['neglog10_pval', 'maf'])
def make_qq_stratified(variant_lines, column_names):
    variants = []
    for variant_line in variant_lines:
        variant = parse_variant_line(variant_line, column_names)
        if variant is not None:
            variants.append(variant)
    variants = sorted(variants, key=lambda v: v.maf)

    # QQ
    num_variants_in_biggest_maf_range = int(math.ceil(len(variants) / NUM_MAF_RANGES))
    max_exp_neglog10_pval = -math.log10(0.5 / num_variants_in_biggest_maf_range) #expected
    max_obs_neglog10_pval = max(v.neglog10_pval for v in variants) #observed
    # TODO: should max_obs_neglog10_pval be at most 9?  That'd break our assertions.
    # print(max_exp_neglog10_pval, max_obs_neglog10_pval)

    qqs = [dict() for i in range(NUM_MAF_RANGES)]
    for qq_i in range(NUM_MAF_RANGES):
        slice_indices = (len(variants) * qq_i//4, len(variants) * (qq_i+1)//NUM_MAF_RANGES - 1)
        qqs[qq_i]['maf_range'] = (variants[slice_indices[0]].maf, variants[slice_indices[1]].maf)
        neglog10_pvals = sorted((v.neglog10_pval for v in variants[slice(*slice_indices)]), reverse=True)
        qqs[qq_i]['count'] = len(neglog10_pvals)

        occupied_bins = set()
        for i, obs_neglog10_pval in enumerate(neglog10_pvals):
            exp_neglog10_pval = -math.log10( (i+0.5) / len(neglog10_pvals))
            exp_bin = int(exp_neglog10_pval / max_exp_neglog10_pval * NUM_BINS)
            obs_bin = int(obs_neglog10_pval / max_obs_neglog10_pval * NUM_BINS)
            occupied_bins.add( (exp_bin,obs_bin) )
        # print(sorted(occupied_bins))

        qq = []
        for exp_bin, obs_bin in occupied_bins:
            assert 0 <= exp_bin <= NUM_BINS, exp_bin
            assert 0 <= obs_bin <= NUM_BINS, obs_bin
            qq.append((
                exp_bin / NUM_BINS * max_exp_neglog10_pval,
                obs_bin / NUM_BINS * max_obs_neglog10_pval
            ))
        qq = sorted(qq)

        qqs[qq_i]['qq'] = qq

    return qqs

'''
def make_qq(variants):
    neglog10_pvals = []
    for variant in variants:
        chrom, pos, ref, alt, maf, pval = parse_variant_tuple(variant)
        neglog10_pvals.append(-math.log10(pval))
    neglog10_pvals = sorted(neglog10_pvals)

    # QQ
    max_exp_neglog10_pval = -math.log10(1/len(neglog10_pvals)) #expected
    max_obs_neglog10_pval = max(neglog10_pvals) #observed
    # print(max_obs_neglog10_pval, max_exp_neglog10_pval)

    occupied_bins = set()
    for i, obs_neglog10_pval in enumerate(neglog10_pvals):
        exp_neglog10_pval = -math.log10((len(neglog10_pvals)-i)/len(neglog10_pvals))
        exp_bin = int(exp_neglog10_pval / max_exp_neglog10_pval * NUM_BINS)
        obs_bin = int(obs_neglog10_pval / max_obs_neglog10_pval * NUM_BINS)
        occupied_bins.add( (exp_bin,obs_bin) )
    # print(sorted(occupied_bins))

    qq = []
    for exp_bin, obs_bin in occupied_bins:
        assert exp_bin <= NUM_BINS, exp_bin
        assert obs_bin <= NUM_BINS, obs_bin
        qq.append((
            exp_bin / NUM_BINS * max_exp_neglog10_pval,
            obs_bin / NUM_BINS * max_obs_neglog10_pval
        ))
    qq = sorted(qq)

    # GC_value lambda
    median_neglog10_pval = neglog10_pvals[len(neglog10_pvals)//2]
    median_pval = 10 ** -median_neglog10_pval # I know, `10 ** -(-math.log10(pval))` is gross.
    gc_value_lambda = gc_value(median_pval)

    rv = {
        'qq': qq,
        'median_pval': median_pval,
        'gc_value_lambda': round_sig(gc_value_lambda, 5),
    }
    return rv
'''



epacts_filename = sys.argv[1]
assert os.path.exists(epacts_filename)
out_filename = sys.argv[2]
assert os.path.exists(os.path.dirname(out_filename))

with gzip.open(epacts_filename) as f:
    header = f.readline().rstrip('\n').split('\t')
    if header[1] == "BEG":
        header[1] = "BEGIN"
    #assert header == ['#CHROM', 'BEGIN', 'END', 'MARKER_ID', 'NS', 'AC', 'CALLRATE', 'MAF', 'PVALUE', 'BETA', 'SEBETA', 'TSTAT', 'R2']

    variant_lines = (line.rstrip('\n') for line in f)
    rv = make_qq_stratified(variant_lines, header)

with open(out_filename, 'w') as f:
    json.dump(rv, f, sort_keys=True, indent=0)
print('{} -> {}'.format(epacts_filename, out_filename))
