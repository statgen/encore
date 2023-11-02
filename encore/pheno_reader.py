import subprocess
import math
import re
import csv
import codecs
from collections import defaultdict, Counter

# These three functions
# guess_raw_type, guess_atomic_column_class, guess_column class
# attempt to identify the type of variable in each of the columns
# of a phenotype file

def atof(text):
    try:
        retval = float(text)
    except ValueError:
        retval = text
    return retval

def guess_raw_type(s):
    # return one of "int","float","str","bool","_empty_"
    if re.match(r'^\s*$', s):
        return "_empty_"
    try:
        f = float(s)
        if "." not in s:
            return "int"
        return "float"
    except ValueError:
        value = s.upper()
        if value =="TRUE" or value == "FALSE":
            return "bool"
        return type(s).__name__

def guess_atomic_column_class(rawtype, obs):
    # rawtype is best guess raw data type
    # obs is a counter (data value, #times occuring) for values
    #   of raw type
    n_vals = sum(obs.values())
    n_uniq_vals = len(obs)
    if n_vals == n_uniq_vals and rawtype != "float":
        if rawtype == "int":
            int_vals = [int(float(x)) for x in obs.elements()]
            int_min = min(int_vals)
            int_max = max(int_vals)
            int_range = int_max - int_min
            if int_min > 1:
                all_same_num_digits = math.floor(math.log10(int_min)) == math.floor(math.log10(int_max))
                bigger_range = math.log10(int_range+1) >  math.log10(n_vals+1) + 2 # 2 order of mag larger
                if not all_same_num_digits and bigger_range:
                    return {"class": "numeric", "type": rawtype}
        return {"class": "id", "type": rawtype}
    if n_uniq_vals == 1:
        return {"class": "fixed", "type": rawtype, "value": next(iter(obs.keys()))}
    if n_uniq_vals == 2:
        levels = list(obs.keys())
        if rawtype in ["int", "float"]:
            levels.sort(key = lambda x: atof(x))
        return {"class": "binary", "type": rawtype, "levels": levels}
    if rawtype == "str":
        n_less_than_five_cats = sum((x<5 for x in obs.values()))
        n_more_than_five_obs = sum((x for x in obs.values() if x>5))
        max_small_group = max(5, math.floor(.25*n_uniq_vals))
        if float(n_more_than_five_obs)/n_vals > .80 and n_less_than_five_cats < max_small_group:
            return {"class": "categorical", "type":"str", "levels": list(obs.keys())}
        else:
            return {"class": "descr", "type": "str"}
    if rawtype == "float" or rawtype == "int":
        return {"class": "numeric", "type": rawtype}
    return {"class": rawtype, "type": rawtype}

def guess_column_class(colinfo):
    # colinfo is dict (types) of counters (values)
    #print(colinfo)
    has_empty = "_empty_" in colinfo and len(colinfo["_empty_"])==1
    col = {k: v for k,v in colinfo.items() if k != "_empty_"}
    n_vals = Counter({k: sum(v.values()) for k, v in col.items()})
    n_uniq_vals = Counter({k: len(v) for k, v in col.items()})
    if len(col)==0:
        #all empty
        return {"class": "fixed", "type": "str"}
    if len(col)==1:
        # all same type
        best_type = n_vals.most_common(1)[0][0]
        ci = guess_atomic_column_class(best_type, col[best_type])
        if has_empty:
            ci["missing"] = next(iter(colinfo["_empty_"].keys()))
        return ci
    if len(col)==2 and "int" in col and "float" in col:
        #promote to float
        best_type = "float"
        vals = col["int"] + col["float"]
        ci =  guess_atomic_column_class(best_type, vals)
        if has_empty:
            ci["missing"] = next(iter(colinfo["_empty_"].keys()))
        return ci
    if len(col)==2 and (n_uniq_vals["str"]==1):
        # likely a single type with a missing indicator
        best_type = [x[0] for x in n_vals.most_common(2) if x[0] != "str"][0]
        ci = guess_atomic_column_class(best_type, colinfo[best_type])
        ci["missing"] = next(iter(colinfo["str"].keys()))
        return ci
    if all((x in ["str","int","float"] for x in col.keys())) and (n_uniq_vals["str"]==1):
        # likely a numeric value with a missing indicator
        best_type = "float"
        vals = col["int"] + col["float"]
        ci = guess_atomic_column_class(best_type, vals)
        ci["missing"] = next(iter(colinfo["str"].keys()))
        return ci
    if len(col)>1 and "str" in col and n_uniq_vals["str"]>2:
        best_type = "str"
        vals = Counter()
        for r in col.values():
            vals += r
        ci = guess_atomic_column_class(best_type, vals)
        if has_empty:
            ci["missing"] = next(iter(colinfo["_empty_"].keys()))
        return ci
    # finally let's just make it a string
    best_type = "str"
    vals = Counter()
    for r in col.values():
        vals += r
    return guess_atomic_column_class(best_type, vals)

def strip_comments(item, token="#"):
    for line in item:
        if not line.strip().startswith(token):
            yield line

def get_comments(item, token="#"):
    print("in csvfile")
    print(item)
    print("token",token)
    for line in item:
        s = line.strip()
        #print("s is",s)
        # if not s.startswith(token) and len(s)>0:

            #raise StopIteration
        yield s

def sniff_file(csvfile):
    chunk = "\n".join([x for _,x in zip(range(50), strip_comments(csvfile))])
    try:
        return csv.Sniffer().sniff(chunk, "\t|, ")
    except:
        return None

def find_header(firstrow, lastcomment, cols):
    colclasses = {k: guess_column_class(v) for k,v in cols.items()}
    col_types = [z["type"] for z in [colclasses[i] for i in range(len(firstrow))]]
    firstrow_types = [guess_raw_type(x) for x in firstrow]

    nonstringcols = sum((x != "str" for x in col_types))
    firstrow_promotions = sum((f=="str" for (f,c) in zip(firstrow_types, col_types) if c!="str"))
    if lastcomment:
        comment_types = [guess_raw_type(x) for x in lastcomment]
        if len(lastcomment) == len(colclasses):
            comment_promotions = sum((f=="str" for (f,c) in zip(comment_types, col_types) if c!="str"))
        else:
            comment_promotions = 0
    else:
        comment_types = []
        comment_promotions = 0

    if nonstringcols > 0 and comment_promotions > firstrow_promotions and (float)(comment_promotions)/nonstringcols > .9:
        #the last comment has the right number of rows and string in non-string columns
        return (lastcomment, "comment")
    if nonstringcols > 0 and (float)(firstrow_promotions)/nonstringcols >= .5:
        return (firstrow, "firstrow")
    # no header found, return unique names
    return (["COL{0}".format(i) for i in range(len(colclasses))], "position")

def check_if_ped(cols, obs):
    # This just isn't working quite right
    # better to check column names?
    return False, None
    if len(cols)<6:
        return False, None
    types = [x["type"] for x in cols]
    classes = [x["class"] for x in cols]
    if not (classes[0] == "id" or classes[1]=="id"):
        return False, None
    if not (types[2] == types[1] or classes[2]=="fixed"):
        return False, None
    if not (types[3] == types[1] or classes[3]=="fixed"):
        return False, None
    if not (classes[4]=="binary" or classes[4]=="fixed"):
        return False, None
    return True, None 

def guess_sample_id_col(metas, cols, known_sample_ids):
    # metas is array (col index) of dict (infered properties) for each column
    # cols is dict (col index) of dict (data type) of counter (values)
    print("metas", metas)
    print("cols",cols)
    print(known_sample_ids)
    best_match = 25 #min overlap
    id_col_idx = None
    for i in range(len(cols)):
        meta = metas[i]
        colinfo = cols[i]
        if meta["class"] == "id" or meta["class"] == "descr":
            if meta["type"] == "str":
                has_dup = colinfo["str"].most_common(1)[0][1]>1
                vals = (x for x in colinfo["str"])
            elif meta["type"] == "int":
                has_dup = colinfo["int"].most_common(1)[0][1]>1
                vals = (str(x) for x in colinfo["int"])
            else:
                continue
            if has_dup:
                continue
            n_matches = sum(x in known_sample_ids for x in vals)
            if n_matches > best_match:
                best_match = n_matches
                id_col_idx = i
    return id_col_idx

def verify_sample_id_col(sample_id_col, metas, cols, known_sample_ids):
    # metas is array (col index) of dict (infered properties) for each column
    # cols is dict (col index) of dict (data type) of counter (values)
    error = ""
    for i in range(len(cols)):
        meta = metas[i]
        colinfo = cols[i]
        if meta["name"] == sample_id_col:
            mtype = meta["type"]
            has_dup = colinfo[mtype].most_common(1)[0][1]>1
            if mtype == "str":
                vals = (x for x in colinfo["str"])
            else:
                vals = (str(x) for x in colinfo[mtype])
            if has_dup:
                return None, "Duplicate IDs detected. IDs must be unique"
            n_matches = sum(x in known_sample_ids for x in vals)
            if known_sample_ids and n_matches == 0:
                return None, "No values overlap known IDs in the genotype data"
            return i, ""
    return None, "No match for column name"

def infer_meta(filepath, dialect=None, known_sample_ids=None, sample_id_column=None):

    meta = {"layout": {}, "columns": []}

    with open(filepath, 'rb') as f:
        first_bytes = f.read(4)

    # assume utf-8 unless BOM found
    encoding ='utf-8'
    for enc, boms in \
            ('utf-8-sig', (codecs.BOM_UTF8,)), \
            ('utf-32', (codecs.BOM_UTF32_LE, codecs.BOM_UTF32_BE)), \
            ('utf-16', (codecs.BOM_UTF16_LE, codecs.BOM_UTF16_BE)):
        if any(first_bytes.startswith(bom) for bom in boms):
            encoding = enc
            break
    meta["encoding"] = encoding

    records = 0
    with open(filepath, 'r', encoding=encoding) as csvfile:
        # store csv dialect
        if not dialect:
            dialect = sniff_file(csvfile)
        for k in [k for k in dir(dialect) if not k.startswith("_")]:
            meta["layout"]["csv_" + k] = getattr(dialect, k)

        # read comments 
        csvfile.seek(0)
        comments = list(get_comments(csvfile))

        # read and process csv rows 
        csvfile.seek(0)
        cvr = csv.reader(strip_comments(csvfile), dialect)
        firstrow = next(cvr)
        cols = defaultdict(lambda : defaultdict(Counter))
        for row in cvr:
            if len(row):
                records += 1
            for idx, val in enumerate(row):
                cols[idx][guess_raw_type(val)][val] += 1
    meta["records"] = records

    # find column headers
    if comments:
        lastcomment = next(csv.reader([comments[-1][1:]], dialect))
    else:
        lastcomment = None
    headers, headersource = find_header(firstrow, lastcomment, cols)
    if headersource == "firstrow":
        skip = len(comments)+1
    else:
        skip = len(comments)
    meta["layout"]["skip"] = skip
    # define columns
    meta["columns"] = [None] * len(cols)
    for col, colval in cols.items():
        coldef = guess_column_class(colval)
        if col < len(headers):
            coldef["name"] = headers[col]
        else:
            coldef["name"] = "COL{}".format(col+1)
        meta["columns"][col] = coldef
    #check if ped
    pedlike, ped_columns = check_if_ped(meta["columns"], cols)
    if pedlike:
        #assign standard ped column types
        meta["pedlike"] = 1
        colclasses = ["family_id","sample_id","father_id","mother_id","sex"]
        for actas, col in zip(colclasses, meta["columns"][0:3]):
            if col["class"] != "fixed":
                col["class"] =  actas
    elif sample_id_column:
        #if ID col specified, verify it's correct
        sample_id_set = set(known_sample_ids)
        id_col, id_error = verify_sample_id_col(sample_id_column, meta["columns"], cols, known_sample_ids=sample_id_set)
        if id_col is not None:
            meta["columns"][id_col]["class"] = "sample_id"
        else:
            meta["id_error"] = id_error
    elif known_sample_ids:
        #if not ped and ID col not specified, try to guess ID column
        id_col =  None
        sample_id_set = set(known_sample_ids)
        id_col = guess_sample_id_col(meta["columns"], cols, known_sample_ids=sample_id_set)
        if id_col is not None:
            meta["columns"][id_col]["class"] = "sample_id"
    return meta

class PhenoReader:
    def __init__(self, path, meta=None):
        self.path = path
        if meta:
            self.meta = meta
        else:
            self.meta = self.infer_meta()

    def infer_meta(self, sample_ids=None, sample_id_column=None):
        return infer_meta(self.path, known_sample_ids=sample_ids, sample_id_column=sample_id_column)

    def get_dialect(self, opts=None):
        class dialect(csv.Dialect):
            pass
        if opts is None and self.meta and "layout" in self.meta:
            opts = {k[4:]:v for (k,v) in self.meta["layout"].items() if k.startswith("csv_")}
            for (k, v) in opts.items():
                setattr(dialect, k, v)
            return dialect
        else:
            return None

    def get_column_missing_values(self):
        cols = self.get_columns();
        return { v["name"]: v.get("missing",None) for v in cols }

    def get_column_indexes(self):
        cols = self.get_columns();
        return { v["name"]: i for (i,v) in enumerate(cols)}

    def get_columns(self):
        if self.meta and self.meta["columns"]:
            return self.meta["columns"]
        else:
            return []

    def get_sample_column_index(self):
        sample_col = next(iter([x for x in self.get_columns() if x["class"]=="sample_id"]), None)
        if not sample_col:
            return None
        return self.get_column_indexes()[sample_col["name"]]

    def row_extractor(self, samples=None):
        dialect = self.get_dialect()
        if "layout" in self.meta and "skip" in self.meta["layout"]:
            skip = self.meta['layout']['skip']
        else:
            skip = 0
        sample_col_idx = self.get_sample_column_index()
        with open(self.path, 'r', encoding=self.meta.get("encoding", "utf-8")) as csvfile:
            if not dialect:
                dialect = sniff_file(csvfile)
                csvfile.seek(0)
            if skip>0:
                [csvfile.readline() for i in range(skip)]
            cvr = csv.reader((row for row in csvfile if not row.startswith("#")), dialect)
            for row in cvr:
                if samples and sample_col_idx is not None and row[sample_col_idx] not in samples:
                    continue
                yield row

    def get_samples(self):
        sample_col_idx = self.get_sample_column_index()
        if sample_col_idx is None:
            return
        for row in self.row_extractor():
            yield row[sample_col_idx]

    @staticmethod
    def get_file_type(file):
        p = subprocess.Popen(["file", "-b", file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        human, err = p.communicate()
        p = subprocess.Popen(["file","-b","-i", file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        mime, err = p.communicate()
        return human.decode().rstrip(), mime.decode().rstrip()

    @staticmethod
    def is_text_file(file):
        human, mime = PhenoReader.get_file_type(file)
        istext = re.search(r"\btext\b", human) is not None
        return istext, human, mime  

if __name__ == "__main__":
    import json
    import sys
    if len(sys.argv)>=2:
        meta = None
        if len(sys.argv)>=3:
            with open(sys.argv[2]) as f:
                meta = json.load(f)
            p = PhenoReader(sys.argv[1], meta)
            print([x for x in p.row_extractor()])
        else:
            p = PhenoReader(sys.argv[1], meta)
            print(json.dumps(p.meta, indent=2))

