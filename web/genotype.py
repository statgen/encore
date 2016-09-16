import os
import json
import sql_pool
import MySQLdb

class Genotype:
    def __init__(self, geno_id, meta=None):
        self.geno_id = geno_id
        self.meta = meta
       
    def getVCFPath(self, chrom):
        vcf_stub = self.meta.get("vcf_path", "vcfs/ALL.chr%(chrom)s.pass.gtonly.genotypes.vcf.gz")
        vcf_path = self.relative_path(vcf_stub % {"chrom": str(chrom)})
        return vcf_path

    def getKinshipPath(self):
        kinship_stub = self.meta.get("kinship_path", "kinship/kinship.kin")
        kinship_path = self.relative_path(kinship_stub)
        return kinship_path

    def getStats(self):
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

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.meta.get("root_path",""), *args))

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
            SELECT bin_to_uuid(id) AS id, name, DATE_FORMAT(creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date 
            FROM genotypes
            WHERE id = uuid_to_bin(%s)
            """
        cur.execute(sql, (geno_id,))
        result = cur.fetchone()
        meta["root_path"] = geno_folder
        meta["name"] = result["name"]
        meta["creation_date"] = result["creation_date"]
        return Genotype(geno_id, meta)
        
    @staticmethod
    def listAll():
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(id) AS id, name, DATE_FORMAT(creation_date, '%Y-%m-%d %H:%i:%s') AS creation_date 
            FROM genotypes 
            ORDER BY creation_date DESC
            """
        cur.execute(sql)
        results = cur.fetchall()
        return results

