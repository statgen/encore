import os
import shutil
from flask import render_template, request, Response, json, current_app, url_for
from flask_login import current_user
from phenotype import Phenotype
from pheno_reader import PhenoReader
from auth import access_pheno_page, check_edit_pheno, can_user_edit_pheno
import sql_pool
import MySQLdb
import uuid



def json_resp(data):
    resp = Response(mimetype='application/json')
    resp.set_data(json.dumps(data))
    return resp

