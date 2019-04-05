#!/usr/bin/env python3



import os.path
import sys
import gzip
import re
import json
import collections
from bisect import bisect_right
from extreme_collection import ExtremeCollection

class NotSortedError(Exception):
    def __init___(self,dErrorArguments):
        Exception.__init__(self,"File Not Sorted By Values: {0}".format(dErrArguments))
        self.dErrorArguments = dErrorArguements

BEDCols = collections.namedtuple('BEDCols', 'start stop name'.split())
class BEDReader:
    def __init__(self, path):
        self.path = path
        self.__load_data()

    def __load_data(self):
        self.data = collections.defaultdict(lambda: BEDCols([],[],[]))
        with open(self.path, "rt") as f:
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
                self.f = gzip.open(self.path, "rt")
            else:
                self.f = open(self.path, "rt")
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

def is_in_bin(rbin, result, window=3e6):
    if result.chrom == rbin['chrom'] or "chr" + result.chrom == rbin['chrom']:
        if rbin['start'] <= result.pos <= rbin['stop']:
            return True
    return False

def process_file(results, window=5e5, sig_pvalue=5e-8, max_sites = 5000, max_bins=500, nearest_gene=None):
    bins=[]
    last_pval = None
    best_results = ExtremeCollection(max_sites, results, lambda x: x.pval)
    exported_cols = ["name","chrom","pos","pval","other"]
    if nearest_gene is not None:
        exported_cols += ["gene"]
    if window>0:
        for result in best_results:
            if last_pval is not None:
                if result.pval < last_pval:
                    raise NotSortedError("input must be sorted by p-value")
            last_pval = result.pval
            for rbin in bins:
                if is_in_bin(rbin, result, window=window):
                    rbin['assoc'].append(result)
                    break 
            else:
                if len(bins) >= max_bins:
                    break
                newbin = dict(chrom = result.chrom,
                    start = int(result.pos-window), 
                    stop = int(result.pos+window),
                    assoc = [result])
                bins.append(newbin)
        for rbin in bins:
            rbin['pos'] = rbin['assoc'][0].pos
            rbin['pval'] = rbin['assoc'][0].pval
            rbin['other'] = rbin['assoc'][0].other
            rbin['sig_count'] = sum(x.pval < sig_pvalue for x in rbin['assoc'])
            rbin['snp_count'] = len(rbin['assoc'])
            del rbin['assoc']
        exported_cols += ["sig_count","snp_count"]
    else:
        bins = [x._asdict() for x in best_results[:max_bins]]
    for result in bins:
        if nearest_gene is not None:
            gene = nearest_gene.get_name(result["chrom"], result["pos"])
            result["gene"] = gene
        if "label" in result["other"]:
            result["name"] = result["other"]["label"]
        else:
            result["name"] = result["chrom"] + ":" + str(result["pos"])
    meta = {"cols": exported_cols}
    return bins, meta

if __name__ == "__main__":
    import argparse
    argp = argparse.ArgumentParser(description='Create JSON file of top hits.')
    argp.add_argument('--bins','-b', help="Maximum number of bins to return", 
        type=int, default=250)
    argp.add_argument('--sites','-s', help="Maximum number of sites to scan", 
        type=int, default=5000)
    argp.add_argument('--sig-pvalue','-p', help="P-value significance threshold", 
        type=float, default = 5e-8)
    argp.add_argument('--window', '-w', help="Window size (in bases) to collapse peaks",
        type=float, default = 5e6)
    argp.add_argument('--gene', '-g', help="BED file for nearest gene")
    argp.add_argument('infile', help="Input file (use '-' for stdin)")
    argp.add_argument('outfile', nargs='?', help="Output file (stdout if not specified)")
    args = argp.parse_args()

    if args.gene:
        nearest_gene = BEDReader(args.gene)
    else:
        nearest_gene = None

    with AssocResultReader(args.infile) as inf, JSONOutFile(args.outfile) as outf:
        bins, meta = process_file(inf, args.window, args.sig_pvalue, args.sites, args.bins, nearest_gene)
        outf.write({"header": meta, "data": bins})

