import argparse
import sqlite3
import csv
import sys

# Define function to create SQLite table
def create_sqlite_table(db_file):
    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()

    # Define the SQL CREATE TABLE statement
    create_table_sql = """
        CREATE TABLE IF NOT EXISTS susieeqtl (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            phenotype_id TEXT NOT NULL,
            variant_id TEXT DEFAULT NULL,
            pip REAL DEFAULT NULL,
            af REAL DEFAULT NULL,
            cs_id INTEGER DEFAULT NULL,
            tissue TEXT DEFAULT NULL
        )
    """

    # Execute the CREATE TABLE statement
    cursor.execute(create_table_sql)

    # Commit and close connection
    conn.commit()
    conn.close()

# Define function to load data from TSV file into SQLite table
def load_data_from_tsv(tsv_file, db_file):
    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()

    # Read data from TSV file and insert into SQLite table
    with open(tsv_file, 'r') as tsvfile:
        reader = csv.reader(tsvfile, delimiter='\t')
        next(reader)  # Skip header row

        for row in reader:
            # Insert data into SQLite table
            insert_sql = """
                INSERT INTO susieeqtl (phenotype_id, variant_id, pip, af, cs_id, tissue)
                VALUES (?, ?, ?, ?, ?, ?)
            """
            cursor.execute(insert_sql, row)

    # Commit and close connection
    conn.commit()
    conn.close()


def create_cond_sqlite_table(db_file):
    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()

    # Define the SQL CREATE TABLE statement
    create_table_sql = """
        CREATE TABLE condeqtl (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    phenotype_id TEXT NOT NULL,
    num_var INTEGER,
    beta_shape1 REAL,
    beta_shape2 REAL,
    true_df REAL,
    pval_true_df TEXT,
    variant_id TEXT,
    tss_distance INTEGER,
    ma_samples INTEGER,
    ma_count INTEGER,
    af REAL,
    pval_nominal TEXT,
    slope REAL,
    slope_se REAL,
    pval_perm TEXT,
    pval_beta TEXT,
    risk TEXT,
    tissue TEXT
);
    """

    # Execute the CREATE TABLE statement
    cursor.execute(create_table_sql)

    # Commit and close connection
    conn.commit()
    conn.close()

# Define function to load data from TSV file into SQLite table
def load_data_from_tsv_condtable(tsv_file, db_file):
    conn = sqlite3.connect(db_file)
    cursor = conn.cursor()

    # Read data from TSV file and insert into SQLite table
    with open(tsv_file, 'r') as tsvfile:
        reader = csv.reader(tsvfile, delimiter='\t')
        next(reader)  # Skip header row

        for row in reader:
            # Insert data into SQLite table
            insert_sql = """
                INSERT INTO condeqtl (phenotype_id, num_var, beta_shape1, beta_shape2, true_df, pval_true_df, variant_id, tss_distance, ma_samples, ma_count, af, pval_nominal, slope, slope_se, pval_perm, pval_beta, risk, tissue)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            """
            cursor.execute(insert_sql, row)

    # Commit and close connection
    conn.commit()
    conn.close()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python createSQliteDb.py <tsv_file> <db_file>")
        sys.exit(1)

    tsv_file = sys.argv[1]
    db_file = sys.argv[2]

    #create_sqlite_table(db_file)
    #load_data_from_tsv(tsv_file, db_file)
    #create_cond_sqlite_table(db_file)
    load_data_from_tsv_condtable(tsv_file, db_file)


#  python plot-epacts-output/createSQliteDb.py  /Users/snehalpatil/Documents/AbecasisLab/eqtl/bravo_eqtl/all.cond.tsv /Users/snehalpatil/Documents/AbecasisLab/GithubEncoreFinal/SingularityBranch/encore/plot-epacts-output/eqtl.db

#sqlite3 /Users/snehalpatil/Documents/AbecasisLab/GithubEncoreFinal/SingularityBranch/encore/plot-epacts-output/eqtl.db




