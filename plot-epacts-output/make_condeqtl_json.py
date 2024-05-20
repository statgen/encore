#!/usr/bin/env python3

import json
import sqlite3
import math

def read_json_file(json_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
    return data

def extract_variant_ids(json_data):
    variant_ids = []

    for entry in json_data['data']:
        p_value= entry['other']['PVALUE']
        if float(p_value) >= 5e-8:
            combined_string = f"{entry['chrom']}_{entry['pos']}_{entry['other']['ref']}_{entry['other']['alt']}"
            variant_ids.append(combined_string)
            # Define the base URL
            base_url = "http://topmedld.csgstat.sph.umich.edu:4546/genome_builds/GRCh38/references/10b92110-3ddc-453d-b3e4-52448b20f384/populations/ALL/variants?correlation=rsquare"
            url_params = {
                'variant': urllib.parse.quote(combined_string),  # Encode special characters in the variant string
                'chrom': entry['chrom'],
                'start': entry['pos'],
                'stop': entry['pos'] + 1  # Assuming stop is pos + 1, adjust as needed
            }

# Define variables for the URL parameters


# Loop through the variables and create the URLs
for variant, chrom, start, stop in zip(variants, chromosomes, starts, stops):
    url = base_url.format(variant, chrom, start, stop)
    print(url)



    #print("The p-value is above 5e-8",p_value)
        else:
            print("pvalue is not above",p_value)




    print(",".join(variant_ids) )
    return variant_ids


def fetch_json(variables):
    json_data = {}
    for var in variables:
        curl_command = f'curl -s "https://example.com/api?var={var}"'
        try:
            result = subprocess.run(curl_command, shell=True, capture_output=True, text=True)
            json_response = json.loads(result.stdout)
            json_data[var] = json_response
        except Exception as e:
            print(f"Error fetching JSON for variable '{var}': {e}")

    return json_data

# Example usage



def convert_to_json(rows):
    # Define the JSON structure
    json_data = {"header": {"cols": ["pheno_id","num_var","true_df","pval_true_df","pval_beta","chr","variant_id","af","tss_distance","risk","tissue"]}, "data": []}

    # Convert each row into a dictionary and append to the "data" list


    for row in rows:
        print (row)
        data_row = {
            "pheno_id": row[0],
            "num_var": row[1],
            "true_df": row[2],
            "pval_true_df": row[3],
            "pval_beta":row[4],
            "chr":row[5].split('_')[0],
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
    print(len(variant_ids))

    variables_list = ['variable1', 'variable2', 'variable3']
    # output_json = fetch_json(variant_ids)
    # print(json.dumps(output_json, indent=4))


#get the json

    #after extracting the variant from top hits data filtert them . Remove the ones which are below 5X10 -8
    #get the list of variant and send it to LS derver. Combine that Variants in the list and check those in eQtls database.


    # Check variants existence in MySQL database

    # rows = check_variants_exist(variant_ids,args.db_file)
    # json_data = convert_to_json(rows)
    # with open(args.output_file, 'w') as f:
    #     json.dump(json_data, f, indent=0)
    #
    #
    # # exported_cols = ["pheno_id","variant_id","pip","af","cs_id","tissue"]
    # # meta = {"cols": exported_cols}
    # #
    #
    # #
    # print(json_data)
    #


#python plot-epacts-output/make_eqtl_json.py --json_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/tophits2.json --db_file /Users/snehalpatil/Documents/AbecasisLab/GithubEncoreFinal/SingularityBranch/encore/plot-epacts-output/eqtl.db --output_file /Users/snehalpatil/Documents/AbecasisLab/encorejobs/34a90c56-dcba-4e05-bd12-698141a1362b/condieqtl.json