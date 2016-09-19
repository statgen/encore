import os
import json
import sql_pool
import MySQLdb

class Phenotype:
    def __init__(self, pheno_id, meta=None):
        self.pheno_id = pheno_id
        self.meta = meta
       
    def getRawPath(self):
        return self.relative_path("pheno.txt")

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.meta.get("root_path",""), *args))

    @staticmethod
    def get(pheno_id, config):
        pheno_folder = os.path.join(config.get("PHENO_DATA_FOLDER", "./"), pheno_id)
        meta_path = os.path.expanduser(os.path.join(pheno_folder, "meta.json"))
        if os.path.exists(meta_path):
            with open(meta_path) as meta_file:
                meta = json.load(meta_file)
        else:
           meta = dict()
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(id) AS id, user_id, name, 
            orig_file_name, md5sum, 
            DATE_FORMAT(creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date 
            FROM phenotypes 
            WHERE id = uuid_to_bin(%s)
            """
        cur.execute(sql, (pheno_id,))
        result = cur.fetchone()
        meta["root_path"] = pheno_folder
        meta["name"] = result["name"]
        meta["orig_file_name"] = result["orig_file_name"]
        meta["md5sum"] = result["md5sum"]
        meta["user_id"] = result["user_id"]
        meta["creation_date"] = result["creation_date"]
        return Phenotype(pheno_id, meta)
