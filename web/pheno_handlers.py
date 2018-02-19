import os
import shutil
from flask import render_template, request, Response, json, current_app, url_for
from flask_login import current_user
from phenotype import Phenotype
from pheno_reader import PhenoReader
from auth import access_pheno_page, check_edit_pheno, can_user_edit_pheno
import sql_pool
import MySQLdb
import hashlib
import uuid


def get_all_phenos():
    phenos = Phenotype.list_all(current_app.config)
    return json_resp(phenos)

def suggest_pheno_name(filename):
    base, ext = os.path.splitext(os.path.basename(filename))
    base = base.replace("_", " ")
    return base

def post_to_pheno():
    user = current_user
    if not user.can_analyze:
        return "User Action Not Allowed", 403
    if request.method != 'POST':
        return json_resp({"error": "NOT A POST REQUEST"}), 405
    if "pheno_file" not in request.files:
        return json_resp({"error": "FILE NOT SENT"}), 400
    pheno_id = str(uuid.uuid4())
    if not pheno_id:
        return json_resp({"error": "COULD NOT GENERATE PHENO ID"}), 500
    pheno_file = request.files["pheno_file"]
    orig_file_name = pheno_file.filename
    pheno_name = suggest_pheno_name(orig_file_name)
    pheno_directory = os.path.join(current_app.config.get("PHENO_DATA_FOLDER", "./"), pheno_id)
    try:
        os.mkdir(pheno_directory)
        pheno_file_path = os.path.join(pheno_directory, "pheno.txt")
        pheno_meta_path = os.path.join(pheno_directory, "meta.json")
        pheno_file.save(pheno_file_path)
        md5 =  hashfile(open(pheno_file_path, "rb")).encode("hex")
    except Exception as e:
        print "File saving error: %s" % e
        return json_resp({"error": "COULD NOT SAVE FILE"}), 500
    # file has been saved to server
    existing_pheno = Phenotype.get_by_hash_user(md5, user.rid, current_app.config)
    if existing_pheno:
        shutil.rmtree(pheno_directory)
        pheno_id = existing_pheno.pheno_id
        pheno_dict = existing_pheno.as_object()
        pheno_dict["id"] = pheno_id
        pheno_dict["url_model"] = url_for("get_model_build", pheno=pheno_id)
        pheno_dict["url_view"] = url_for("get_pheno", pheno_id=pheno_id)
        pheno_dict["existing"] = True
        return json_resp(pheno_dict)
    # file has not been uploaded before
    istext, filetype, mimetype = PhenoReader.is_text_file(pheno_file_path)
    if not istext:
        shutil.rmtree(pheno_directory)
        return json_resp({"error": "NOT A RECOGNIZED TEXT FILE",
            "filetype": filetype,
            "mimetype": mimetype}), 400
    try:
        db = sql_pool.get_conn()
        cur = db.cursor()
        sql = """
            INSERT INTO phenotypes (id, user_id, name, orig_file_name, md5sum)
            VALUES (UNHEX(REPLACE(%s,'-','')), %s, %s, %s, %s)
            """
        cur.execute(sql, (pheno_id, user.rid, pheno_name, orig_file_name, md5))
        db.commit()
    except Exception as e:
        print "Databse error: %s" % e
        shutil.rmtree(pheno_directory)
        return json_resp({"error": "COULD NOT SAVE TO DATABASE"}), 500
    # file has been saved to DB
    pheno = PhenoReader(pheno_file_path)
    if pheno.meta:
        meta = pheno.meta
    else:
        meta = pheno.infer_meta()
    line_count = sum(1 for _ in pheno.row_extractor()) 
    meta["records"] = line_count
    with open(pheno_meta_path, "w") as f:
        json.dump(meta, f)
    return json_resp({"id": pheno_id,  \
        "url_model": url_for("get_model_build", pheno=pheno_id), \
        "url_view": url_for("get_pheno", pheno_id=pheno_id)})

def purge_pheno(pheno_id):
    result = Phenotype.purge(pheno_id, current_app.config)
    if result["found"]:
        return json_resp(result)
    else:
        return json_resp(result), 404

def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp

def hashfile(afile, hasher=None, blocksize=65536):
    if not hasher:
        hasher = hashlib.md5()
    buf = afile.read(blocksize)
    while len(buf) > 0:
        hasher.update(buf)
        buf = afile.read(blocksize)
    return hasher.digest()
