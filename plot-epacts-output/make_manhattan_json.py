#!/usr/bin/env python2

'''
This script takes two arguments:
- an input filename (the output of epacts with a single phenotype)
- an output filename (please end it with `.json`)

It creates a json file which can be used to render a Manhattan plot.
'''

from __future__ import print_function, division, absolute_import

import os.path
import sys
import gzip
import re
import json
import math
import collections

BIN_LENGTH = int(3e6)
NEGLOG10_PVAL_BIN_SIZE = 0.05 # Use 0.05, 0.1, 0.15, etc
NEGLOG10_PVAL_BIN_DIGITS = 2 # Then round to this many digits
BIN_THRESHOLD = 1e-4 # pvals less than this threshold don't get binned.

def round_sig(x, digits):
    return 0 if x==0 else round(x, digits-1-int(math.floor(math.log10(abs(x)))))
assert round_sig(0.00123, 2) == 0.0012
assert round_sig(1.59e-10, 2) == 1.6e-10


def parse_marker_id(marker_id):
    try:
        chr1, pos1, ref, alt, opt_info = re.match(r'([^:]+):([0-9]+)_([-ATCG]+)/([-ATCG]+)(?:_(.+))?', marker_id).groups()
        #assert chr1 == chr2
        #assert pos1 == pos2
    except:
        print(marker_id)
        raise
    return chr1, int(pos1), ref, alt

Variant = collections.namedtuple('Variant', 'chrom pos ref alt maf pval beta sebeta'.split())
def parse_variant_line(variant_line):
    v = variant_line.split('\t')
    #assert v[1] == v[2]
    if v[8] == 'NA' or v[9] == 'NA':
        assert v[8] == 'NA' and v[9] == 'NA'
    else:
        chrom, pos, maf, pval, beta, sebeta = v[0], int(v[1]), float(v[7]), float(v[8]), float(v[9]), float(v[10])
        chrom2, pos2, ref, alt = parse_marker_id(v[3])
        assert chrom == chrom2
        assert pos == pos2
        return Variant(chrom, pos, ref, alt, maf, pval, beta, sebeta)

def rounded_neglog10(pval):
    return round(-math.log10(pval) // NEGLOG10_PVAL_BIN_SIZE * NEGLOG10_PVAL_BIN_SIZE, NEGLOG10_PVAL_BIN_DIGITS)

def get_pvals_and_pval_extents(pvals):
    # expects that NEGLOG10_PVAL_BIN_SIZE is the distance between adjacent bins.
    pvals = sorted(pvals)
    extents = [[pvals[0], pvals[0]]]
    for p in pvals:
        if extents[-1][1] + NEGLOG10_PVAL_BIN_SIZE * 1.1 > p:
            extents[-1][1] = p
        else:
            extents.append([p,p])
    rv_pvals, rv_pval_extents = [], []
    for (start, end) in extents:
        if start == end:
            rv_pvals.append(start)
        else:
            rv_pval_extents.append([start,end])
    return (rv_pvals, rv_pval_extents)

def bin_variants(variant_lines):
    bins = []
    unbinned_variants = []

    prev_chrom, prev_pos = -1, -1
    for variant_line in variant_lines:
        variant = parse_variant_line(variant_line)
        if variant is None: continue
        #assert variant.pos >= prev_pos or int(variant.chrom) > int(prev_chrom), (variant.chrom, variant.pos, prev_chrom, prev_pos) # variant.chrom is not always an integer (eg, X).
        prev_chrom, prev_pos = variant.chrom, variant.pos

        if variant.pval < BIN_THRESHOLD:
            unbinned_variants.append({
                'chrom': variant.chrom,
                'pos': variant.pos,
                'ref': variant.ref,
                'alt': variant.alt,
                'maf': round_sig(variant.maf, 3),
                'pval': round_sig(variant.pval, 2),
                'beta': round_sig(variant.beta, 2),
                'sebeta': round_sig(variant.sebeta, 2),
            })

        else:
            if len(bins) == 0 or variant.chrom != bins[-1]['chrom']:
                # We need a new bin, starting with this variant.
                bins.append({
                    'chrom': variant.chrom,
                    'startpos': variant.pos,
                    'neglog10_pvals': set(),
                })
            elif variant.pos > bins[-1]['startpos'] + BIN_LENGTH:
                # We need a new bin following the last one.
                bins.append({
                    'chrom': variant.chrom,
                    'startpos': bins[-1]['startpos'] + BIN_LENGTH,
                    'neglog10_pvals': set(),
                })
            bins[-1]['neglog10_pvals'].add(rounded_neglog10(variant.pval))

    bins = [b for b in bins if len(b['neglog10_pvals']) != 0]
    for b in bins:
        b['neglog10_pvals'], b['neglog10_pval_extents'] = get_pvals_and_pval_extents(b['neglog10_pvals'])
        b['pos'] = int(b['startpos'] + BIN_LENGTH/2)
        del b['startpos']

    return bins, unbinned_variants


epacts_filename = sys.argv[1]
assert os.path.exists(epacts_filename)
out_filename = sys.argv[2]
assert os.path.exists(os.path.dirname(out_filename))

with gzip.open(epacts_filename) as f:
    header = f.readline().rstrip('\n').split('\t')
    assert header == ['#CHROM', 'BEGIN', 'END', 'MARKER_ID', 'NS', 'AC', 'CALLRATE', 'MAF', 'PVALUE', 'BETA', 'SEBETA', 'TSTAT', 'R2']

    variant_lines = (line.rstrip('\n') for line in f)
    variant_bins, unbinned_variants = bin_variants(variant_lines)

rv = {
    'variant_bins': variant_bins,
    'unbinned_variants': unbinned_variants,
}

# Avoid getting killed while writing dest_filename, to stay idempotent despite me frequently killing the program
with open(out_filename, 'w') as f:
    json.dump(rv, f, sort_keys=True, indent=0)
print('{} -> {}'.format(epacts_filename, out_filename))
