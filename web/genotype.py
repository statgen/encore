import os
import json
import sql_pool
import MySQLdb
from pheno_reader import PhenoReader
from geno_reader import GenoReader

class Genotype:
    def __init__(self, geno_id, meta=None):
        self.geno_id = geno_id
        if meta is None:
            self.meta = {}
        else:
            self.meta = meta
        self.name = None
        self.build = None
        self.creation_date = None
        self.root_path = ""
       
    def get_vcf_path(self, chrom=1, must_exist=False):
        vcf_stub = ""
        chrom = str(chrom)
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

    def get_info_stats(self):
        if "info_stats_path" in self.meta:
            stats_stub = self.meta.get("info_stats_path", "info.json")
            stats_path = self.relative_path(stats_stub)
            with open(stats_path) as infile:
                stats = json.load(infile)
            return stats
        else:
            return dict()

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

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.root_path, *args))

    def as_object(self):
        obj = {"geno_id": self.geno_id, "name": self.name, "build": self.build}
        obj["stats"] = self.get_stats()
        obj["phenos"] = self.get_phenotypes()
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
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(id) AS id, name, build, 
            DATE_FORMAT(creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date 
            FROM genotypes
            WHERE id = uuid_to_bin(%s)
            """
        cur.execute(sql, (geno_id,))
        result = cur.fetchone()
        if result is not None:
            g = Genotype(geno_id, meta)
            g.name = result["name"]
            g.build = result["build"]
            g.creation_date = result["creation_date"]
            g.root_path = geno_folder
        else:
            g = None
        return g
        
    @staticmethod
    def list_all():
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(id) AS id, name, build, DATE_FORMAT(creation_date, '%Y-%m-%d %H:%i:%s') AS creation_date 
            FROM genotypes 
            ORDER BY creation_date DESC
            """
        cur.execute(sql)
        results = cur.fetchall()
        return results

