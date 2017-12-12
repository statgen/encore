import re
import time
import os
import glob

def bin_chunks_by_chr_and_age(chunks, now):
    def get_age_group(a):
        diff_minutes = (now-a)/60
        if diff_minutes < 10:
            return 0
        elif diff_minutes < 60: 
            return 1
        else:
            return 2

    bins = {}
    for chunk in chunks:
        chunk_chr = chunk["chrom"]
        chunk_age  = get_age_group(chunk["modified"])
        chunk_key = (chunk_chr, chunk_age)
        #chunk_key = "{}-{}".format(chunk_chr, chunk_age)
        if chunk_key in bins:
            bins[chunk_key]["vals"].append(chunk)
        else:
            bins[chunk_key] = {
                "chrom": chunk_chr,
                "age": chunk_age,
                "vals": [chunk]
            }

    def sort_chunk_vals(chunk):
        chunk["vals"] = sorted(chunk["vals"], key=lambda x: x["start"] )
        return chunk

    bins = { k: sort_chunk_vals(v) for (k,v) in bins.iteritems() }
    return bins

def collapse_chunk_bins(bins):
    results = []
    state = { "start": 0, "stop": 0, "oldest": time.gmtime(), "newest": time.gmtime() }

    def add(chrom, age, state):
        results.append({"chrom": chrom, "age": age, 
            "start": state["start"], "stop": state["stop"],
            "oldest": state["oldest"], "newest": state["newest"] })
    def reset(val, state):
        state["start"]= val["start"]
        state["stop"] = val["stop"]
        state["oldest"] = val["modified"]
        state["newest"] = val["modified"]
        return state
    def extend(val, state):
        state["stop"] = val["stop"]
        state["oldest"] = val["modified"] if val["modified"]<state["oldest"] else state["oldest"]
        state["newest"] = val["modified"] if val["modified"]>state["newest"] else state["newest"]
        return state

    for b in bins.itervalues():
        vals = iter(b["vals"])
        state = reset(next(vals), state)
        for val in vals:
            if val["start"] != state["stop"]+1:
                add(b["chrom"], b["age"], state)
                state = reset(val, state)
            else: 
                state = extend(val, state)
        add(b["chrom"], b["age"], state)

    return results

def get_gene_chunk_progress(output_file_glob, input_file_glob):
    in_files = glob.glob(input_file_glob)
    out_files = glob.glob(output_file_glob)
    return {"data": {"total": len(in_files), "complete": len(out_files)}, 
        "header": {"format": "progress"}}

def get_chr_chunk_progress(output_file_glob, fileregex):
    files = glob.glob(output_file_glob)
    now = time.mktime(time.localtime())
    if len(files):
        chunks = []
        p = re.compile(fileregex)
        for file in files:
            m = p.search(file)
            chunk = dict(m.groupdict())
            chunk['chrom'] =  chunk['chr']
            chunk['start'] = int(chunk['start'])
            chunk['stop'] = int(chunk['stop'])
            chunk['modified'] = time.mktime(time.localtime(os.path.getmtime(file)))
            chunks.append(chunk)
        
        result = collapse_chunk_bins(bin_chunks_by_chr_and_age(chunks, now))
        return {"data": result, "header": {"format": "ideogram"}}

            

