import json
from flask import current_app
import sqlite3


class JSONOutFile:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        if self.path:
            assert os.path.exists(os.path.dirname(os.path.abspath(self.path)))
            self.f = open(self.path, 'w')
        else:
            self.f = sys.stdout
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdout:
            self.f.close()

    def write(self, data):
        json.dump(data, self.f, indent=0)

def read_json_file(json_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
    return data

def extract_variant_ids(json_data):
    variant_ids = []
    for entry in json_data['data']:

        combined_string = f"{entry['chrom']}_{entry['pos']}_{entry['other']['ref']}_{entry['other']['alt']}"
        variant_ids.append(combined_string)

    #print(",".join(variant_ids) )
    return variant_ids

def convert_to_json(rows):
    # Define the JSON structure
    json_data = {"header": {"cols": ["pheno_id","variant_id","maxpip","avgpip","maxaf","af","cs_id","tissue"]}, "data": []}

    # Convert each row into a dictionary and append to the "data" list
    for row in rows:
        print (row)
        data_row = {
            "variant_id": row[0],
            "pheno_id": row[1],
            "maxpip": row[2],
            "avgpip": row[3],
            "maxaf": row[4],
            "af": row[5],
            "cs_id": row[6],
            "tissue": row[7]
        }
        json_data["data"].append(data_row)



    return json_data


def check_variants_exist(variant_ids,db_file):

    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()

    sql_query = sql_query = '''
   SELECT 
        main.variant_id, 
        main.phenotype_id, 
        main.max_pip, 
        main.avg_pip, 
        main.max_af, 
        main.avg_af, 
        main.max_cs_id,
        GROUP_CONCAT(sub.tissue) AS tissues
    FROM 
        (SELECT 
            variant_id, 
            phenotype_id, 
            MAX(pip) AS max_pip, 
            AVG(pip) AS avg_pip, 
            MAX(af) AS max_af, 
            AVG(af) AS avg_af, 
            MAX(cs_id) AS max_cs_id
        FROM 
            susieeqtl      
        WHERE 
             variant_id in  ({})
	    GROUP BY 
            variant_id, 
            phenotype_id) AS main
    JOIN
        susieeqtl AS sub
    ON 
        main.variant_id = sub.variant_id
    AND
        main.phenotype_id = sub.phenotype_id
    GROUP BY 
        main.variant_id, 
        main.phenotype_id, 
        main.max_pip, 
        main.avg_pip, 
        main.max_af, 
        main.avg_af, 
        main.max_cs_id
'''.format(','.join(['?']*len(variant_ids)))

    cursor.execute(sql_query, variant_ids)


    # Execute the query with the variant IDs as parameters
    #cursor.execute(query, variant_ids)
    rows = cursor.fetchall()

    conn.close()
    print(rows)

    return rows



if __name__ == "__main__":
    import argparse

    #argp = argparse.ArgumentParser(description='Read JSON file and check variants in MySQL database.')
    argp = argparse.ArgumentParser(description='Read JSON file and check variants in SQLite database.')
    argp.add_argument('--json_file','-jfile', help="Input JSON file", default='tophits.json')
    argp.add_argument('--db_file','-dbfile', help="SQLite database file", default='eqtl.db')
    argp.add_argument('--output_file','-outfile', help="SQLite output file", default='susieeqtl.json')
    args = argp.parse_args()


# Read JSON file
    json_data = read_json_file(args.json_file)

    # Extract variant IDs from JSON
    variant_ids = extract_variant_ids(json_data)

    # Check variants existence in MySQL database

    rows = check_variants_exist(variant_ids,args.db_file)
    exported_cols = ["pheno_id","variant_id","pip","af","cs_id","tissue"]
    meta = {"cols": exported_cols}

    json_data = convert_to_json(rows)

    print(json_data)

    with open(args.output_file, 'w') as f:
        json.dump(json_data, f, indent=0)

#python make_eqtl_json.py --json_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/tophits2.json --db_file /Users/snehalpatil/Documents/AbecasisLab/GithubEncoreFinal/SingularityBranch/encore/plot-epacts-output/eqtl.db
#plot-epacts-output snehalpatil$ python make_eqtl_json.py --json_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/tophits2.json --db_file /Users/snehalpatil/Documents/AbecasisLab/GithubEncoreFinal/SingularityBranch/encore/plot-epacts-output/eqtl.db --output_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/susieeqtl.json
