import os
import shutil
import json
import sql_pool
import MySQLdb
from pheno_reader import PhenoReader

class Phenotype:
    __dbfields = ["name","orig_file_name","md5sum","user_id","creation_date"]

    def __init__(self, pheno_id, meta=None):
        self.pheno_id = pheno_id
        self.name = None
        self.orig_file_name = None
        self.md5sum = None
        self.user_id = None
        self.creation_date = None
        self.root_path = "" 
        self.meta = meta
       
    def get_raw_path(self):
        return self.relative_path("pheno.txt")

    def get_pheno_reader(self):
        return PhenoReader(self.get_raw_path(), self.meta)

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.root_path, *args))

    def as_object(self):
        obj = {key: getattr(self, key) for key in self.__dbfields if hasattr(self, key)} 
        obj["pheno_id"] = self.pheno_id
        obj["meta"] = self.meta
        return obj

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
        if result is not None:
            p = Phenotype(pheno_id, meta)
            p.root_path = pheno_folder
            map(lambda x: setattr(p, x, result[x]), \
                (val for val in Phenotype.__dbfields if val in result))
            return p
        else:
            return None
    
    @staticmethod
    def list_all_for_user(user_id, config=None):
        db = sql_pool.get_conn()
        cur = db.cursor(MySQLdb.cursors.DictCursor)
        sql = """
            SELECT bin_to_uuid(id) AS id, user_id, name,
            orig_file_name, md5sum, 
            DATE_FORMAT(creation_date, '%%Y-%%m-%%d %%H:%%i:%%s') AS creation_date 
            FROM phenotypes
            WHERE user_id = %s
            ORDER BY creation_date DESC
            """
        cur.execute(sql, (user_id,))
        results = cur.fetchall()
        return results

    @staticmethod
    def purge(pheno_id, config=None):
        pheno = Phenotype.get(pheno_id, config)
        result = {}
        if pheno:
            db = sql_pool.get_conn()
            cur = db.cursor()
            sql = "DELETE FROM phenotypes WHERE id = uuid_to_bin(%s)"
            cur.execute(sql, (pheno_id, ))
            affected = cur.rowcount
            db.commit()

            removed = False
            pheno_directory = pheno.root_path 
            if os.path.isdir(pheno_directory) and affected>0:
                try:
                    shutil.rmtree(pheno_directory)
                    removed = True
                except:
                    pass

            result = {"phenos": affected, "files": removed, "found": True}
            return result
        else:
            return {"found": False}
