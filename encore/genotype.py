import os
import json
from . import sql_pool
import MySQLdb
from collections import OrderedDict
from .pheno_reader import PhenoReader
from .geno_reader import GenoReader
from .db_helpers import SelectQuery, TableJoin, PagedResult, OrderClause, OrderExpression, WhereExpression, WhereAll

class Genotype:
    __dbfields = ["id", "name", "build", "is_active", "creation_date"]

    def __init__(self, geno_id, meta=None):
        self.geno_id = geno_id
        if meta is None:
            self.meta = {}
        else:

            self.meta = meta
        self.name = None
        self.build = None
        self.creation_date = None
        self.is_active = None
        self.root_path = ""
        self.build_info = {} 
       
    def get_vcf_path(self, chrom=1, must_exist=False):
        vcf_stub = ""
        chrom = str(chrom).replace("chr","")
        if "vcfs" in self.meta:
            vcfs = self.meta["vcfs"]
            if type(vcfs) is dict:
                if chrom in vcfs:
                    vcf_stub = vcfs[chrom]
                elif "*" in vcfs:
                    vcf_stub = vcfs["*"]
            else:
                vcf_stub = vcfs
        if vcf_stub == "":
            vcf_stub = "vcfs/chr{0}.vcf.gz"
        vcf_stub = vcf_stub.replace("*","{0}").format(chrom)
        vcf_path = self.relative_path(vcf_stub)
        if must_exist and not os.path.exists(vcf_path):
            return None
        return vcf_path

    def get_vcf_anno_path(self, chrom=1, must_exist=False):
        anno_stub = ""
        chrom = str(chrom).replace("chr","")
        if "annovcfs" in self.meta:
            anno = self.meta["annovcfs"]
            if type(anno) is dict:
                if chrom in anno:
                    anno_stub = anno[chrom]
                elif "*" in anno:
                    anno_stub = anno["*"]
            else:
                anno_stub = anno
        anno_stub = anno_stub.replace("*","{0}").format(chrom)
        if anno_stub == "":
            return None
        anno_path = self.relative_path(anno_stub)
        if must_exist and not os.path.exists(anno_path):
            return None
        return anno_path

    def get_sav_path(self, chrom=1, must_exist=False):
        sav_stub = ""
        chrom = str(chrom).replace("chr","")
        if "savs" in self.meta:
            savs = self.meta["savs"]
            if type(savs) is dict:
                if chrom in savs:
                    sav_stub = savs[chrom]
                elif "*" in savs:
                    sav_stub = savs["*"]
            else:
                sav_stub = savs
        if sav_stub == "":
            sav_stub = "savs/chr{0}.sav"
        sav_stub = sav_stub.replace("*","{0}").format(chrom)
        sav_path = self.relative_path(sav_stub)
        if must_exist and not os.path.exists(sav_path):
            return None
        return sav_path

    def get_groups_path(self, group, must_exist=False):
        grp_stub = ""
        if "groups" in self.meta:
            groups = self.meta["groups"]
            if type(groups) is dict:
                if group in groups:
                    grp_stub = groups[group]
                elif "*" in groups:
                    grp_stub = groups["*"]
            else:
                grp_stub = groups
        if grp_stub == "":
            grp_stub = "groups/{0}.grp"
        grp_stub = grp_stub.replace("*","{0}").format(group)
        grp_path = self.relative_path(grp_stub)
        if must_exist and not os.path.exists(grp_path):
            return None
        return grp_path

    def get_kinship_path(self, must_exist=False):
        kinship_stub = self.meta.get("kinship_path", "kinship/kinship.kin")
        kinship_path = self.relative_path(kinship_stub)
        if must_exist and not os.path.exists(kinship_path):
            return None
        return kinship_path

    def get_stats(self):
        if "stats" in self.meta:
           return self.meta["stats"] 
        elif "stats_path" in self.meta:
            stats_stub = self.meta.get("stats_path", "stats.json")
            stats_path = self.relative_path(stats_stub)
            with open(stats_path) as infile:
                stats = json.load(infile)
            self.meta["stats"] = stats
            return stats
        return dict() 

    def get_readme(self):
        if "readme" in self.meta:
           return self.meta["readme"]
        elif "readme_path" in self.meta:
            readme_stub = self.meta.get("readme_path")
            readme_path = self.relative_path(readme_stub)
            if os.path.isfile(readme_path):
                with open(readme_path) as infile:
                    readme = infile.read()
                self.meta["readme"] = readme
                return readme
        return ""

    def get_chromosomes(self):
        if "chrs" in self.meta:
            chrs = self.meta["chrs"]
            if chrs == "autosomes":
                chrs = " ".join(map(str, range(1,23)))
            elif chrs == "autosomesX":
                chrs = " ".join(map(str, range(1,23))) + " X"
            return chrs
        return None

    def get_chromosome_ranges(self):
        if "chr_ranges" in self.meta:
            chr_ranges = self.meta["chr_ranges"]
            if type(chr_ranges) == str:
                path = self.relative_path(chr_ranges)
                if not os.path.exists(path):
                    raise Exception("chr_ranges path not found: {}".format(path))
                chrom_list = []
                with open(path, "rt") as lines:
                    for line in lines:
                        if line.startswith("#"):
                            continue
                        chrom, start, stop = line.split()[0:3]
                        start = int(start)
                        stop = int(stop)
                        chrom_list.append({"chrom": chrom, "start": start, "stop": stop})
                return chrom_list
            else:
                raise Exception("Unexpected type for chr_ranges: {}".format(type(chr_ranges)))
        return None

    def get_info_stats(self):
        if "info_stats_path" in self.meta:
            stats_stub = self.meta.get("info_stats_path", "info.json")
            stats_path = self.relative_path(stats_stub)
            with open(stats_path) as infile:
                stats = json.load(infile)
            return stats
        else:
            return dict()

    def get_samples(self):
        path = self.get_samples_path()
        if path:
            with open(path) as infile:
                for sample in infile:
                    yield sample.rstrip("\n")

    def get_samples_path(self):
        samples_path = self.meta.get("samples_path", "")
        samples_path = self.relative_path(samples_path)
        if os.path.isfile(samples_path):
            return samples_path
        else:
            return None

    def get_pca_genotypes_path(self, must_exist=False):
        geno_path = self.meta.get("pca_genotypes_path", None)
        if not geno_path:
            return None
        geno_path = self.relative_path(geno_path)
        if must_exist and not os.path.exists(geno_path):
            return None
        return geno_path

    def get_build_info(self, config, build):
        if build in config.get("BUILD_REF", {}):
            build_info = config.get("BUILD_REF").get(build)
        else:
            raise Exception("Build information not found: {}".format(build))
        return build_info

    def get_build_ref_path(self):
        return self.build_info.get("fasta", None)

    def get_build_nearest_gene_path(self):
        return self.build_info.get("nearest_gene_bed", None)

    def get_phenotypes(self):
        if not "phenotypes" in self.meta:
            return None
        phenos = self.meta["phenotypes"]
        if not isinstance(phenos, list):
            phenos = [phenos]
        result = []
        for p in phenos:
            pmeta = None
            if "meta" in p:
                with open(self.relative_path(p["meta"])) as f:
                    pmeta = json.load(f)
            result.append({"name": p.get("name","pheno"), "meta": pmeta})
        return result

    def get_pheno_reader(self, index=0):
        if not "phenotypes" in self.meta:
            return None
        phenos = self.meta["phenotypes"]
        if not isinstance(phenos, list):
            phenos = [phenos]
        p = phenos[index]
        pmeta = None
        if "meta" in p:
            with open(self.relative_path(p["meta"])) as f:
                pmeta = json.load(f)
        return PhenoReader(self.relative_path(p["file"]), pmeta)

    def get_geno_reader(self, config):
        return GenoReader(self, config)

    def get_ld_info(self, config):
        if not "ld_server" in self.meta:
            return None
        metald = self.meta["ld_server"]
        ld_panel = self.geno_id
        ld_population = "ALL"
        ld_build = self.build
        if type(metald) == bool:
            if not metald:
                return None
        elif type(metald) == str:
            ld_panel = metald
        elif isinstance(metald, dict):
            ld_panel = metald.get("panel", ld_panel)
            ld_population = metald.get("population", ld_population)
            ld_build = metald.get("build", ld_build)
        else:
            raise Exception("Unrecognized type for ld_server config:" + type(metald))
        return {"panel": ld_panel, "population": ld_population,
            "build": ld_build}

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.root_path, *args))

    def as_object(self, include_meta=False):
        obj = {"geno_id": self.geno_id, 
            "name": self.name, 
            "build": self.build, 
            "creation_date": self.creation_date,
            "is_active": self.is_active}
        obj["stats"] = self.get_stats()
        obj["phenos"] = self.get_phenotypes()
        obj["readme"] = self.get_readme()
        avail = dict()
        avail["vcf"] = True if self.get_vcf_path(must_exist=True) else False
        avail["sav"] = True if self.get_sav_path(must_exist=True) else False
        avail["kinship"] = True if self.get_kinship_path(must_exist=True) else False
        avail["snps"] = True if self.get_pca_genotypes_path(must_exist=True) else False
        avail["group_nonsyn"] = True if self.get_groups_path("nonsyn", must_exist=True) else False
        obj["avail"] = avail
        if include_meta:
            obj["meta"] = self.meta
        return obj

    @staticmethod
    def get(geno_id, config):
        geno_folder = os.path.join(config.get("GENO_DATA_FOLDER", "./"), geno_id)
        meta_path = os.path.expanduser(os.path.join(geno_folder, "meta.json"))
        if os.path.exists(meta_path):
            with open(meta_path) as meta_file:
                meta = json.load(meta_file)
        else:
           meta = dict()
        db = sql_pool.get_conn()
        results = Genotype.__list_by_sql_where(db, "id=uuid_to_bin(%s)", (geno_id,))
        result = results[0] if results else None
        if result is not None:
            g = Genotype(geno_id, meta)
            g.name = result["name"]
            g.build = result["build"]
            g.creation_date = result["creation_date"]
            g.is_active = result["is_active"]
            g.root_path = geno_folder
            g.build_info = g.get_build_info(config, g.build)
        else:
            g = None
        return g
        
    @staticmethod
    def list_all_for_user(user_id=None, config=None, query=None):
        db = sql_pool.get_conn()
        where = WhereExpression("is_active=1")
        results = Genotype.__list_by_sql_where_query(db, where=where, query=query)
        return results

    @staticmethod
    def list_all(config=None, query=None):
        db = sql_pool.get_conn()
        results = Genotype.__list_by_sql_where_query(db, query=query)
        return results

    @staticmethod
    def __list_by_sql_where(db, where="", vals=()):
        where = WhereExpression(where, vals)
        result = Genotype.__list_by_sql_where_query(db, where=where, query=None)
        return result.results

    @staticmethod
    def __list_by_sql_where_query(db, where=None, query=None):
        cols = OrderedDict([("id", "bin_to_uuid(genotypes.id)"),
            ("name", "genotypes.name"),
            ("build", "genotypes.build"),
            ("creation_date", "DATE_FORMAT(genotypes.creation_date, '%%Y-%%m-%%d %%H:%%i:%%s')"),
            ("is_active", "genotypes.is_active")])
        qcols = ["name", "build", "creation_date"]
        page, order_by, qsearch = SelectQuery.translate_query(query, cols, qcols)
        if not order_by:
            order_by = OrderClause(OrderExpression(cols["creation_date"], "DESC"))
        sqlcmd = (SelectQuery()
            .set_cols([ "{} AS {}".format(v,k) for k,v in cols.items()])
            .set_table("genotypes")
            .set_where(where)
            .set_search(qsearch)
            .set_order_by(order_by)
            .set_page(page))
        return PagedResult.execute_select(db, sqlcmd)

    @staticmethod
    def create(new_values, db=None, config=None):
        updateable_fields = [x for x in Genotype.__dbfields]
        required_fields = ["id", "name", "build"]
        fields = list(new_values.keys()) 
        values = list(new_values.values())
        bad_fields = [x for x in fields if x not in updateable_fields]
        if len(bad_fields)>0:
            raise Exception("Invalid field: {}".format(", ".join(bad_fields)))
        missing_fields = [x for x in required_fields if x not in fields]
        if len(missing_fields)>0:
            raise Exception("Missing required fields: {}".format(", ".join(missing_fields)))
        empty_fields = [x for x in required_fields if len(new_values[x])<1]
        if len(empty_fields)>0:
            raise Exception("Missing values for fields: {}".format(", ".join(empty_fields)))
        builds = config.get("BUILD_REF", {}).keys()
        if new_values["build"] not in config.get("BUILD_REF", {}).keys():
            raise Exception("Unrecognized build: {} (known: {})".format(
                new_values["build"], ", ".join(builds)))
        if db is None:
            db = sql_pool.get_conn()
        sql = "INSERT INTO genotypes (" + \
            ", ".join(fields)+ \
            ") values (" + \
            ", ".join(["uuid_to_bin(%s)" if x=="id" else "%s" for x in fields]) + \
            ")"
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        cur.execute(sql, values)
        db.commit()
        new_geno = Genotype.get(new_values["id"], config=config)
        result = {"geno": new_geno}
        return result

