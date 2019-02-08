#!/usr/bin/env python3

'''
This script takes two arguments:
- an input filename (the output of epacts with a single phenotype)
- an output filename (please end it with `.json`)

It creates a json file which can be used to render a QQ plot.
'''



import os.path
import sys
import gzip
import re
import json
import math
import collections
import scipy.stats
from itertools import islice

NEGLOG10_PVAL_BIN_SIZE = 0.05 # Use 0.05, 0.1, 0.15, etc
NEGLOG10_PVAL_BIN_DIGITS = 2 # Then keep 2 digits after the decimal

def round_sig(x, digits):
    return 0 if x==0 else round(x, digits-1-int(math.floor(math.log10(abs(x)))))
assert round_sig(0.00123, 2) == 0.0012
assert round_sig(1.59e-10, 2) == 1.6e-10

def approx_equal(a, b, tolerance=1e-4):
    return abs(a-b) <= max(abs(a), abs(b)) * tolerance
assert approx_equal(42, 42.0000001)
assert not approx_equal(42, 42.01)


def gc_value(median_pval):
    # This should be equivalent to this R: `qchisq(p, df=1, lower.tail=F) / qchisq(.5, df=1, lower.tail=F)`
    return scipy.stats.chi2.ppf(1 - median_pval, 1) / scipy.stats.chi2.ppf(0.5, 1)
assert approx_equal(gc_value(0.49), 1.047457) # I computed these using that R code.
assert approx_equal(gc_value(0.5), 1)
assert approx_equal(gc_value(0.50001), 0.9999533)
assert approx_equal(gc_value(0.6123), 0.5645607)

def rounded(x):
    return round(x // NEGLOG10_PVAL_BIN_SIZE * NEGLOG10_PVAL_BIN_SIZE, NEGLOG10_PVAL_BIN_DIGITS)

def get_conf_int(nvar):
    slices = []
    for x in range(0, int(math.ceil(math.log(nvar,2)))):
        slices.append(2**x)
    slices.append(nvar-1);
    slices.reverse()

    points = []
    for slice in slices:
        rv = scipy.stats.beta(slice, nvar-slice)
        points.append((
            round(-math.log10((slice-0.5)/nvar),2),
            round(-math.log10(rv.ppf(0.05/2)),2), 
            round(-math.log10(rv.ppf(1-(0.05/2))),2)
        ))
    return points

def make_qq(variants, max_unbinned, num_bins):
    # smallest first
    sorted_variants = sorted((x for x in variants if x is not None), key=lambda x: x.pval)
    count = len(sorted_variants)

    max_exp_neglog10_pval = -math.log10(0.5 / count) #expected
    max_obs_neglog10_pval = -math.log10(sorted_variants[0].pval )

    current_bin = -1
    variants_in_bin = []
    binned_variants = []
    for i in range(max_unbinned+1, count):
        exp_neglog10_pval = -math.log10( (i+0.5) / count ) 
        exp_bin = int(exp_neglog10_pval / max_exp_neglog10_pval * num_bins)
        obs_neglog10_pval = -math.log10( sorted_variants[i].pval )
        if exp_bin != current_bin:
            if len(variants_in_bin)>0:
                binned_variants.append((
                    round_sig(current_bin / num_bins * max_exp_neglog10_pval, 3),
                    round_sig(sum(variants_in_bin) / float(len(variants_in_bin)), 3) 
                ))
                variants_in_bin = []
            current_bin = exp_bin
        variants_in_bin.append(obs_neglog10_pval)
    if len(variants_in_bin)>0:
        binned_variants.append((
            round_sig(current_bin / num_bins * max_exp_neglog10_pval, 3),
            round_sig(sum(variants_in_bin) / float(len(variants_in_bin)), 3) 
        ))

    unbinned_variants = []
    for i in range(0, min(max_unbinned, count)):
        variant = sorted_variants[i]
        obs_neglog10_pval = -math.log10( variant.pval )
        exp_neglog10_pval = -math.log10( (i+0.5) / count )
        rec = {
            'chrom': variant.chrom,
            'pos': variant.pos,
            'pval': variant.pval
        }
        rec.update(variant.other)
        unbinned_variants.append( (round_sig(exp_neglog10_pval,3), round_sig(obs_neglog10_pval,3), rec) )

    median_pval = sorted_variants[count//2].pval
    gc = {
        "50": round_sig(gc_value(median_pval),5) 
    }
    conf_int = get_conf_int(count)
        
    return {"variant_bins": binned_variants, "unbinned_variants": unbinned_variants, 
        "gc": gc, "count": count, "conf_int": conf_int} 


def process_file_unbinned(results, max_unbinned, num_bins):
    plot = {"description": "Overall Results", "layers": [make_qq(results, max_unbinned, num_bins)]}
    return plot

def process_file_quartile_binned(results, max_unbinned, num_bins):
    maf_sorted = sorted((x for x in results if x is not None), key=lambda v: float(v.other["MAF"]))
    count = len(maf_sorted)
    num_blocks = 4
    block_base_size = count // num_blocks 
    ranges = [[i*block_base_size, (i+1)*block_base_size-1] for i in range(num_blocks)]
    ranges[num_blocks-1][1] += count - block_base_size * num_blocks 
    blocks = []
    for i in range(num_blocks):
        start = ranges[i][0]
        stop = ranges[i][1]
        maf_range = [float(maf_sorted[start].other["MAF"]), float(maf_sorted[stop].other["MAF"])]
        block = make_qq(islice(maf_sorted, start, stop), max_unbinned, num_bins)
        block["maf_range"] = maf_range 
        block["level"] = "Q{}".format(i)
        blocks.append(block)
    plot = {"description": "MAF Stratified", "layers": blocks}
    return plot

def process_file(results, max_unbinned, num_bins):
    if "MAF" in results.filecols:
        plot = process_file_quartile_binned(results, max_unbinned, num_bins)
    else:
        plot = process_file_unbinned(results, max_unbinned, num_bins)
    return {"header": {"variant_columns": list(results.filecols.keys())}, "data": [plot]} 

AssocResult = collections.namedtuple('AssocResult', 'chrom pos pval other'.split())
class AssocResultReader:

    _single_id_regex = re.compile(r'([^:]+):([0-9]+)_([-ATCG]+)\/([-ATCG]+)(?:_(.+))?')
    _group_id_regex = re.compile(r'([^:]+):([0-9]+)-([0-9]+)(?:_(.+))?')

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
        self.itr = iter(self.f)
        header = next(self.itr)
        self.__parseheader(header)
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdin:
            self.f.close()

    def __parseheader(self, line):
        if line.startswith("#"):
            line = line[1:]
        header = line.rstrip().split()
        aliases = {"BEG": "BEGIN",
            "CHR": "CHROM",
            "POS": "BEGIN",
            "SNPID": "MARKER_ID",
            "N": "NS",
            "p.value": "PVALUE"}
        for i, col in enumerate(header):
            if aliases.get(col):
                header[i] = aliases.get(col)
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
            if pval < 1e-308:
                pval = 1e-308
            marker_id = v[column_indices["MARKER_ID"]]
            other = { k: v[i] for k,i in column_indices.items()};
            match = AssocResultReader._single_id_regex.match(marker_id)
            if match:
                chrom2, pos2, ref2, alt2, name2 = match.groups()
                assert chrom == chrom2
                assert pos == int(pos2)
                other["ref"] = ref2
                other["alt"] = alt2
                if name2:
                    other["label"] = name2
            else:
                match = AssocResultReader._group_id_regex.match(marker_id)
                if match:
                    chrom2, begin2, end2, name2 = match.groups()
                    other["start"] = begin2
                    other["stop"] = end2
                    if name2:
                        other["label"] = name2
            return AssocResult(chrom, pos, pval, other)

    def __iter__(self):
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

if __name__ == "__main__":

    import argparse
    argp = argparse.ArgumentParser(description='Create JSON file for manhattan plot.', \
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    argp.add_argument('--max-unbinned','-M', help="Maximum number of variants to return", 
        type=int, default=50)
    argp.add_argument('--pval-bins','-b', help="Number of exp p-value bins", 
        type=int, default=1000)
    argp.add_argument("--maf-breaks", help="MAF breaks for stratified results",
        type=str, default = "")
    argp.add_argument('infile', help="Input file (use '-' for stdin)")
    argp.add_argument('outfile', nargs='?', help="Output file (stdout if not specified)")
    args = argp.parse_args()

    with AssocResultReader(args.infile) as inf, JSONOutFile(args.outfile) as outf:
        bins = process_file(inf, args.max_unbinned, args.pval_bins)  
        outf.write(bins)

    print('{} -> {}'.format(args.infile, args.outfile))
