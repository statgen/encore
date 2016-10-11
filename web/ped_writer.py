from pipes import quote # use shlex.quote(s) for python 3.3+

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
        if self.coldef:
            self.name = coldef["name"]
            self.colindex = [x["name"] for x in pr.meta["columns"]].index(self.name)
            self.missing = coldef.get("missing", None)

    def headers(self):
        return [self.name] 

    def values(self, row):
        val = row[self.colindex]
        if self.missing is not None and val==self.missing:
            return None
        return [row[self.colindex]] 

class CategoricalColumn(Column):
    def __init__(self, coldef, pr):
        super(CategoricalColumn, self).__init__(coldef, pr)
        self.levels = coldef["levels"]
        self.ref_level = self.levels[0]
        self.contr_levels = self.levels[1:-1]

    def headers(self):
        return [self.name + "_" + x for x in self.contr_levels]

    def values(self, row):
        val = super(CategoricalColumn, self).values(row)
        if val is None:
            return None
        return [str(int(val[0]==x)) for x in self.contr_levels] 

class BinaryColumn(Column):
    
    def __init__(self, coldef, pr):
        super(BinaryColumn, self).__init__(coldef, pr)
        self.levels = coldef["levels"]
        self.ref_level = self.levels[0]

    def headers(self):
        return [self.name + "_" + self.ref_level]

    def values(self, row):
        val = super(BinaryColumn, self).values(row)
        if val is None:
            return None
        return [str(int(val[0]==self.ref_level))] 

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

    def values(self, row):
        if self.coldef is None:
            return ["0"]
        return super(PedRequiredColumn, self).values(row)

def flatten(x):
    #return [item for sublist in x for item in sublist]
    return reduce(lambda a,b: a+b if b is not None else a + [None], x, [])

class PedWriter:
    def __init__(self, phenoreader, resp, covar):
        self.pr = phenoreader
        pedcols = ["family_id", "sample_id", "father_id", "mother_id", "sex"]
        self.pedcols = [ColumnFactory.get_by_special_class(x, self.pr) for x in pedcols]
        self.respcols = [ColumnFactory.get_by_name(resp, self.pr)]
        self.covarcols = [ColumnFactory.get_by_name(x, self.pr) for x in covar]
        self.allcols = self.pedcols + self.respcols + self.covarcols

        def uniqueify(vals, existing):
            taken = existing[:]
            uniqued = []
            for val in vals:
                newval = quote(val)
                ind = 1
                while newval in taken:
                    newval = quote(val + "." + ind)
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

    def write_to_file(self, fconn):
        row_count = 0
        fconn.write("#" + "\t".join(self.headers) + "\n")
        for row in self.pr.row_extractor():
            vals = flatten([x.values(row) for x in self.allcols])
            has_missing = any((x is None for x in vals))
            if not has_missing:
                fconn.write("\t".join(vals) + "\n")
                row_count += 1
        return row_count
