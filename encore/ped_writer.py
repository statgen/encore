import re

def sanitize(x):
    if re.match(r'^[^A-Za-z]', x):
        x = "X" + x
    x = re.sub(r'[^A-Za-z0-9._]', "_", x)
    return x

class ColumnFactory:
    @staticmethod
    def get_by_name(name, pr):
        coldef = next((x for x in pr.meta["columns"] if x["name"]==name), None)
        return ColumnFactory.__get_column_class(coldef, pr) 

    @staticmethod
    def get_by_special_class(colclass, pr):
        coldef = next((x for x in pr.meta["columns"] if x["class"]==colclass), None)
        if colclass in ["family_id","sample_id","father_id","mother_id","sex"]:
            return PedRequiredColumn(coldef, colclass, pr)
        else:
            return None

    @staticmethod
    def find_id_column(cols):
        col = next((x for x in cols if x.coldef and x.coldef["class"] == "sample_id"), None)
        return col 

    @staticmethod
    def __get_column_class(coldef, pr):
        if not "class" in coldef:
            raise Exception("Invalid Column Definition")
        colclass = coldef["class"]
        if colclass=="categorical":
            return CategoricalColumn(coldef, pr)
        elif colclass=="binary":
            return BinaryColumn(coldef, pr)
        else:
            return Column(coldef, pr)

class Column(object):
    def __init__(self, coldef, pr):
        self.coldef = coldef
        self._raw_values = []
        if self.coldef:
            self.name = coldef["name"]
            self.colindex = [x["name"] for x in pr.meta["columns"]].index(self.name)
            self.missing = coldef.get("missing", None)

    def headers(self):
        return [self.name] 

    def append(self, row):
        val = row[self.colindex]
        if self.missing is not None and val==self.missing:
            val = None
        self._raw_values.append(val)
        return val

    def value(self, index):
        val = self._raw_values[index] 
        return val

    def values(self, index):
        val = self.value(index)
        if val is None:
            return None
        return [val]

    def reorder(self, indexlist):
        new_raw_values = []
        for idx in indexlist:
            if idx is not None:
                new_raw_values.append(self._raw_values[idx])
            else:
                new_raw_values.append(None)
        self._raw_values = new_raw_values

    def __len__(self):
        return len(self._raw_values)

class CategoricalColumn(Column):
    def __init__(self, coldef, pr):
        super(CategoricalColumn, self).__init__(coldef, pr)
        self.levels = coldef["levels"]
        self.ref_level = self.levels[0]
        self.contr_levels = self.levels[1:]

    def headers(self):
        return [self.name + "_" + x for x in self.contr_levels]

    def values(self, index):
        val = self.value(index) 
        if val is None:
            return None
        ret = ["0"] * len(self.contr_levels)
        if val == self.ref_level:
            return ret
        for index, level in enumerate(self.contr_levels):
            if val == level:
                ret[index] = "1"
                return ret
        raise Exception("Found unexpected value ({}) in categorical column ()".format(val, self.name))

class BinaryColumn(Column):
    
    def __init__(self, coldef, pr):
        super(BinaryColumn, self).__init__(coldef, pr)
        self.levels = coldef["levels"]
        self.ref_level = self.levels[0]
        self.alt_level = self.levels[1]

    def headers(self):
        return [self.name + "_" + self.alt_level]

    def values(self, index):
        val = self.value(index) 
        if val is None:
            return None
        if val==self.ref_level:
            return ["0"]
        elif val==self.alt_level:
            return ["1"]
        else:
            raise Exception("Found unexpected value in binary column")

class PedRequiredColumn(Column):
    def __init__(self, coldef, field, pr):
        super(PedRequiredColumn, self).__init__(coldef, pr)
        self.field = field

    def headers(self):
        header = ""
        if self.field == "family_id":
            header = "FAM_ID"
        elif self.field == "sample_id":
            header = "IND_ID"
        elif self.field == "father_id":
            header = "FAT_ID"
        elif self.field == "mother_id":
            header = "MOT_ID"
        elif self.field == "sex":
            header = "SEX"
        else:
            header = self.field
        return [header]

    def append(self, row):
        if self.coldef is None or self.colindex is None:
            return None
        return super(PedRequiredColumn, self).append(row)

    def values(self, index):
        if self.coldef is None:
            return ["0"]
        return super(PedRequiredColumn, self).values(index)

def flatten(x):
    #return [item for sublist in x for item in sublist]
    return reduce(lambda a,b: a+b if b is not None else a + [None], x, [])

class PedWriter:
    def __init__(self, phenoreader=None, resp=None, covar=None):
        pedcols = ["family_id", "sample_id", "father_id", "mother_id", "sex"]
        self.pedcols = [ColumnFactory.get_by_special_class(x, phenoreader) for x in pedcols]
        self.respcols = [ColumnFactory.get_by_name(resp, phenoreader)]
        self.covarcols = [ColumnFactory.get_by_name(x, phenoreader) for x in covar]
        self.allcols = self.pedcols + self.respcols + self.covarcols
        for row in phenoreader.row_extractor():
            for col in self.allcols:
                col.append(row)
        self.expand_columns()

    def merge_covar(self, phenoreader=None, covar=None):
        datacols =  [ColumnFactory.get_by_name(x, phenoreader) for x in covar]
        matchcol = ColumnFactory.get_by_special_class("sample_id", phenoreader)
        lookup = dict()
        for idx, row in enumerate(phenoreader.row_extractor()):
            id = matchcol.append(row)
            lookup[id] = idx
            for col in datacols:
                col.append(row)

        reindex = []
        matchcol = ColumnFactory.find_id_column(self.allcols)
        for idx in range(len(matchcol)):
            sample = matchcol.value(idx)
            if sample in lookup:
                reindex.append(lookup[sample])
            else:
                reindex.append(None)

        for col in datacols:
            col.reorder(reindex)

        self.covarcols += datacols
        self.allcols += datacols
        self.expand_columns()

    def expand_columns(self):

        def uniqueify(vals, existing):
            taken = existing[:]
            uniqued = []
            for val in vals:
                newval = sanitize(val)
                ind = 1
                while newval in taken:
                    newval = sanitize(val + "." + ind)
                uniqued.append(newval)
            assert len(vals) == len(uniqued)
            return uniqued

        self.pedheaders = uniqueify(flatten([x.headers() for x in self.pedcols]), [])
        self.headers = self.pedheaders
        self.respheaders = uniqueify(flatten([x.headers() for x in self.respcols]), self.headers)
        self.headers = self.headers + self.respheaders
        self.covarheaders = uniqueify(flatten([x.headers() for x in self.covarcols]), self.headers)
        self.headers = self.headers + self.covarheaders

    def get_response_headers(self):
        return self.respheaders

    def get_covar_headers(self):
        return self.covarheaders

    def write_to_file(self, fconn, comment_header=True):
        row_count = 0
        self.expand_columns()
        header = "\t".join(self.headers) + "\n"
        if comment_header:
            header = "#" + header
        fconn.write(header)
        for idx in range(max((len(x) for x in self.allcols))):
            vals = flatten([x.values(idx) for x in self.allcols])
            has_missing = any((x is None for x in vals))
            if not has_missing:
                fconn.write("\t".join(vals) + "\n")
                row_count += 1
        return row_count

if __name__ == "__main__":
    from pheno_reader import PhenoReader
    import json
    import os
    import sys

    class screenout:
        def write(self, txt):
            sys.stdout.write(txt)

    def init(filename):
        pr = PhenoReader(filename)
        meta = pr.infer_meta()
        print json.dumps(meta)

    def get_pr(filename):
        meta = None
        with open(filename.replace(".txt",".json")) as f:
            meta = json.load(f)
        return PhenoReader(filename, meta)

    #init(os.path.expanduser("~/in2.txt"))
    pr1 = get_pr(os.path.expanduser("~/in1.txt"))
    pr2 = get_pr(os.path.expanduser("~/in2.txt"))
    pedw = PedWriter(pr1, "col3",["col4","col2"])
    sout = screenout();
    pedw.write_to_file(sout)
    pedw.merge_covar(pr2, ["pc3", "pc1"])
    pedw.write_to_file(sout)
