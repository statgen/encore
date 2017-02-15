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
import heapq

def round_sig(x, digits):
    return 0 if x==0 else round(x, digits-1-int(math.floor(math.log10(abs(x)))))
assert round_sig(0.00123, 2) == 0.0012
assert round_sig(1.59e-10, 2) == 1.6e-10

_single_id_regex = re.compile(r'([^:]+):([0-9]+)_([-ATCG]+)\/([-ATCG]+)(?:_(.+))?')
_group_id_regex = re.compile(r'([^:]+):([0-9]+)-([0-9]+)(?:_(.+))?')

def rounded_neglog10(pval, neglog10_pval_bin_size, neglog10_pval_bin_digits):
    return round(-math.log10(pval) // neglog10_pval_bin_size * neglog10_pval_bin_size, neglog10_pval_bin_digits)

def get_pvals_and_pval_extents(pvals, neglog10_pval_bin_size):
    # expects that NEGLOG10_PVAL_BIN_SIZE is the distance between adjacent bins.
    pvals = sorted(pvals)
    extents = [[pvals[0], pvals[0]]]
    for p in pvals:
        if extents[-1][1] + neglog10_pval_bin_size * 1.1 > p:
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

def bin_variants(variants, bin_length, binning_pval_threshold, n_unbinned, neglog10_pval_bin_size, neglog10_pval_bin_digits):
    bins = {} 
    unbinned_variant_heap = []
    exports = [["ref","ref"], ["alt","alt"], ["MAF","maf"],
        ["BETA","beta"],["SEBETA","sebeta"], ["label","label"], ["NS","N"]]
    chrom_order = {}
    chrom_n_bins = {}

    def box(x):
        return (-x.pval, x)

    def unbox(x):
        return x[1]

    def bin_variant(variant):
        chrom = variant.chrom
        if not chrom in chrom_order:
            chrom_order[chrom] = len(chrom_order)
        chrom_key = chrom_order[chrom]
        pos_bin = variant.pos // bin_length
        chrom_n_bins[chrom_key] = max(chrom_n_bins.get(chrom_key,0), pos_bin)
        if (chrom_key, pos_bin) in bins:
            bin = bins[(chrom_key, pos_bin)]
        else:
            bin = {"chrom": chrom,
                   "startpos": pos_bin * bin_length,
                   "neglog10_pvals": set()}
            bins[(chrom_key, pos_bin)] = bin
        bin["neglog10_pvals"].add(rounded_neglog10(variant.pval, neglog10_pval_bin_size, neglog10_pval_bin_digits))
        
    variant_iterator = iter((v for v in variants if v)) 
    # put the most-significant variants into the heap and bin the rest
    for variant in variant_iterator:
        if variant.pval > binning_pval_threshold:
            bin_variant(variant)
        else:
            heapq.heappush(unbinned_variant_heap, box(variant))
            if len(unbinned_variant_heap) > n_unbinned:
                old = heapq.heappop(unbinned_variant_heap)
                bin_variant(unbox(old))

    unbinned_variants = []
    for variant in (unbox(x) for x in unbinned_variant_heap):
        rec = {
            'chrom': variant.chrom,
            'pos': variant.pos,
            'pval': round_sig(variant.pval, 2)
        }
        for field, export_as in exports:
            if field in variant.other:
                rec[export_as] = variant.other[field]
        unbinned_variants.append(rec)


    # unroll bins into simple array (preserving chromosomal order)
    binned_variants = []
    for chrom_key in sorted(chrom_order.values()):
        for pos_key in xrange(int(1+chrom_n_bins[chrom_key])):
            b = bins.get((chrom_key, pos_key), None)
            if b and len(b['neglog10_pvals']) != 0:
                b['neglog10_pvals'], b['neglog10_pval_extents'] = get_pvals_and_pval_extents(b['neglog10_pvals'], neglog10_pval_bin_size)
                b['pos'] = int(b['startpos'] + bin_length/2)
                del b['startpos']
                binned_variants.append(b)

    return binned_variants, unbinned_variants

AssocResult = collections.namedtuple('AssocResult', 'chrom pos pval other'.split())
class AssocResultReader:
    def __init__(self, path):
        self.path = path
        self.filecols = dict()

    def __enter__(self):
        if self.path and self.path != "-":
            if self.path.endswith(".gz"):
                self.f = gzip.open(self.path)
            else:
                self.f = open(self.path)
        else:
            self.f = sys.stdin
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdin:
            self.f.close()

    def __parseheader(self, line):
        if line.startswith("#"):
            line = line[1:]
        header = line.rstrip().split()
        if header[1] == "BEG":
            header[1] = "BEGIN"
        self.filecols = { x:i for i,x in enumerate(header)}

    def row_parser(self, row):
        column_indices = self.filecols
        v = row.rstrip().split('\t')
        if v[column_indices["PVALUE"]] == 'NA':
            return None
        else:
            chrom = v[column_indices["CHROM"]]
            pos = int(v[column_indices["BEGIN"]])
            pval = float(v[column_indices["PVALUE"]])
            marker_id = v[column_indices["MARKER_ID"]]
            other = { k: v[i] for k,i in column_indices.iteritems()};
            match = _single_id_regex.match(marker_id)
            if match:
                chrom2, pos2, ref2, alt2, name2 = match.groups()
                assert chrom == chrom2
                assert pos == int(pos2)
                other["ref"] = ref2
                other["alt"] = alt2
                if name2:
                    other["label"] = name2
            else:
                match = _group_id_regex.match(marker_id)
                if match:
                    chrom2, begin2, end2, name2 = match.groups()
                    other["start"] = begin2
                    other["stop"] = end2
                    if name2:
                        other["label"] = name2
            return AssocResult(chrom, pos, pval, other)

    def __iter__(self):
        self.itr = iter(self.f)
        header = next(self.itr)
        self.__parseheader(header)
        return self

    def __next__(self):
        line = next(self.itr)
        return self.row_parser(line)

    next = __next__

class JSONOutFile:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        if self.path:
            assert os.path.exists(os.path.dirname(os.path.abspath(self.path)))
            self.f = open(self.path, 'w')
        else:
            self.f = sys.stdout
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdout:
            self.f.close()

    def write(self, data):
        json.dump(data, self.f, indent=0)

def process_file(results, bin_length, bin_threshold, max_unbinned, neglog10_pval_bin_size, neglog10_pval_bin_digits):
    import time
    def prog_printer(iterable, stepsize=int(1e6)):
        t1 = time.time()
        for i, it in enumerate(iterable):
            if i % stepsize == 0:
                t2 = time.time()
                print('processed {:15,} in {:.2f} seconds'.format(i, t2-t1))
                t1 = t2
            yield it
        print('processed {:15,} in {:.2f} seconds'.format(i, time.time()-t1))

    with results as f:
        variants = f
        variants = prog_printer(variants)
        variant_bins, unbinned_variants = bin_variants(variants, bin_length, \
            bin_threshold, max_unbinned, neglog10_pval_bin_size, neglog10_pval_bin_digits)

    rv = {
        'variant_bins': variant_bins,
        'unbinned_variants': unbinned_variants,
    }
    print('num unbinned:', len(unbinned_variants))
    return rv

if __name__ == "__main__":
    import argparse
    argp = argparse.ArgumentParser(description='Create JSON file for manhattan plot.', \
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    argp.add_argument('--max-unbinned','-M', help="Maximum number of unbinned variants to return", 
        type=int, default=500)
    argp.add_argument('--pval-round','-r', help="Number of digits to round binned p-values", 
        type=int, default=2)
    argp.add_argument('--pval-window','-p', help="Size of p-value bins", 
        type=float, default = 0.05)
    argp.add_argument('--sig-pvalue','-P', help="P-value significance threshold for binning" + \
        " (any p-value above this will automatically be binned)", type=float, default = 1)
    argp.add_argument('--pos-window', '-w', help="Window size (in bases) to collapse peaks",
        type=float, default = 3e6)
    argp.add_argument('infile', help="Input file (use '-' for stdin)")
    argp.add_argument('outfile', nargs='?', help="Output file (stdout if not specified)")
    args = argp.parse_args()

    with AssocResultReader(args.infile) as inf, JSONOutFile(args.outfile) as outf:
        bins = process_file(inf, args.pos_window, args.sig_pvalue, args.max_unbinned, \
            args.pval_window, args.pval_round)
        outf.write(bins)

    print('{} -> {}'.format(args.infile, args.outfile))
