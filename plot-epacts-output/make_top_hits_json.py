#!/usr/bin/env python2

from __future__ import print_function, division, absolute_import

import os.path
import sys
import gzip
import re
import json
import collections
from bisect import bisect_right

BEDCols = collections.namedtuple('BEDCols', 'start stop name'.split())
class BEDReader:
    def __init__(self, path):
        self.path = path
        self.__load_data()

    def __load_data(self):
        self.data = collections.defaultdict(lambda: BEDCols([],[],[]))
        with open(self.path) as f:
            for line in f:
                cols = line.split()
                if cols[0]=="browser" or cols[0]=="track" or cols[0].startswith("#"):
                   continue 
                self.data[cols[0]].start.append(int(cols[1]))
                self.data[cols[0]].stop.append(int(cols[2]))
                self.data[cols[0]].name.append(cols[3])

    def get_name(self, chrom, pos):
        if chrom not in self.data:
            chrom = "chr" + chrom
        if chrom in self.data:
            i = bisect_right(self.data[chrom].start, pos)
            if i>0:
                i = i-1
                if self.data[chrom].stop[i] >= pos:
                    return self.data[chrom].name[i]
        return None


AssocResult = collections.namedtuple('AssocResult', 'chrom pos ref alt pval name'.split())
class AssocResultReader:
    def __init__(self, path):
        self.path = path
        self.filecols = dict()

    def __enter__(self):
        if self.path:
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
        self.filecols = { x:i for i,x in enumerate(line.split())}


    def row_parser(self, row):
        cols = row.split("\t")
        marker_id = cols[self.filecols["MARKER_ID"]]
        chrom, pos, ref, alt, name = re.match(r'([^:]+):([0-9]+)_([-ATCG]+)/([-ATCG]+)(?:_(.+))?', marker_id).groups()
        pval = cols[self.filecols["PVALUE"]]
        return AssocResult(chrom, int(pos), ref, alt, float(pval), name)

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
            self.f = open(self.path, 'w')
        else:
            self.f = sys.stdout
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdout:
            self.f.close()

    def write(self, data):
        json.dump(data, self.f, indent=0)

def is_in_bin(rbin, result, window=3e6):
    if result.chrom == rbin['chrom'] or "chr" + result.chrom == rbin['chrom']:
        if rbin['start'] <= result.pos <= rbin['stop']:
            return True
    return False

def process_file(results, output, window=5e5, sig_pvalue=5e-8, max_bins=500, nearest_gene=None):
    bins=[]
    for result in results:
        for rbin in bins:
            if is_in_bin(rbin, result, window=window):
                rbin['assoc'].append(result)
                break 
        else:
            if len(bins) >= max_bins:
                break
            newbin = dict(chrom = result.chrom,
                peak = result.pos,
                pval = result.pval,
                start = int(result.pos-window), 
                stop = int(result.pos+window),
                assoc = [result])
            if result.name is not None:
                newbin['name'] = result.name
            else:
                newbin['name'] = result.chrom + ":" + str(result.pos)
            if nearest_gene is not None:
                gene = nearest_gene.get_name(result.chrom, result.pos)
                newbin['gene'] = gene
            bins.append(newbin)
    for rbin in bins:
        rbin['sig_count'] = sum(x.pval < sig_pvalue for x in rbin['assoc'])
        del rbin['assoc']
    output.write(bins)

if __name__ == "__main__":
    import argparse
    argp = argparse.ArgumentParser(description='Create JSON file of top hits.')
    argp.add_argument('--bins','-b', help="Maximum number of bins to return", 
        type=int, default=500)
    argp.add_argument('--sig-pvalue','-p', help="P-value significance threshold", 
        type=float, default = 5e-8)
    argp.add_argument('--window', '-w', help="Window size (in bases) to collapse peaks",
        type=float, default = 5e5)
    argp.add_argument('--gene', '-g', help="BED file for nearest gene")
    argp.add_argument('--out', '-o', help="Outout file (stdout if not specified)", dest="outfile")
    argp.add_argument("infile", nargs='?', help="Input file (stdin if not specified)")
    args = argp.parse_args()

    if args.gene:
        nearest_gene = BEDReader(args.gene)
    else:
        nearest_gene = None

    with AssocResultReader(args.infile) as inf, JSONOutFile(args.outfile) as outf:
        process_file(inf, outf, args.window, args.sig_pvalue, args.bins, nearest_gene)
