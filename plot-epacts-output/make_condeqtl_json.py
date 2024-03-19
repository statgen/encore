import json
import sqlite3




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
    #'(205055, 'ENSG00000243417', 7912, 1.02885, 1078.06, 5732.29, '1.26305e-05', 'chr4_152691391_A_G', 139027, 389, 397, 0.0300666, '3.5149e-06', -0.135487, 0.0291859, '0.0108989', '0.0117985', '11', 'Whole_blood')
    json_data = {"header": {"cols": ["pheno_id","num_var","true_df","pval_true_df","pval_beta","variant_id","af","tss_distance","risk","tissue"]}, "data": []}

    # Convert each row into a dictionary and append to the "data" list


    for row in rows:
        print (row)
        data_row = {
            "pheno_id": row[0],
            "num_var": row[1],
            "true_df": row[2],
            "pval_true_df": row[3],
            "pval_beta":row[4],
            "variant_id": row[5],
            "af": row[6],
            "tss_distance": row[7],
            "risk": row[8],
            "tissue": row[9]
        }
        json_data["data"].append(data_row)



    return json_data


def check_variants_exist(variant_ids,db_file):

    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()

    sql_query = sql_query = '''
   SELECT 
      phenotype_id,num_var,true_df,pval_true_df, pval_beta,variant_id,af,tss_distance,risk,tissue
    FROM 
    condeqtl where variant_id in ({})
'''.format(','.join(['?']*len(variant_ids)))

    cursor.execute(sql_query, variant_ids)




    # Execute the query with the variant IDs as parameters
    #cursor.execute(query, variant_ids)
    rows = cursor.fetchall()

    conn.close()
    print("******************************************")
    print(rows)
    print("******************************************")

    return rows



if __name__ == "__main__":
    import argparse

    #argp = argparse.ArgumentParser(description='Read JSON file and check variants in MySQL database.')
    argp = argparse.ArgumentParser(description='Read JSON file and check variants in SQLite database.')
    argp.add_argument('--json_file','-jfile', help="Input JSON file", default='tophits.json')
    argp.add_argument('--db_file','-dbfile', help="SQLite database file", default='eqtl.db')
    argp.add_argument('--output_file','-outfile', help="SQLite output file", default='condeeqtl.json')
    args = argp.parse_args()


# Read JSON file
    json_data = read_json_file(args.json_file)

    # Extract variant IDs from JSON
    variant_ids = extract_variant_ids(json_data)

    # Check variants existence in MySQL database

    rows = check_variants_exist(variant_ids,args.db_file)
    # exported_cols = ["pheno_id","variant_id","pip","af","cs_id","tissue"]
    # meta = {"cols": exported_cols}
    #
    json_data = convert_to_json(rows)
    #
    print(json_data)
    #
    with open(args.output_file, 'w') as f:
        json.dump(json_data, f, indent=0)

#python plot-epacts-output/make_eqtl_json.py --json_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/tophits2.json --db_file /Users/snehalpatil/Documents/AbecasisLab/GithubEncoreFinal/SingularityBranch/encore/plot-epacts-output/eqtl.db --output_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/condieqtl.json