from ped_writer import PedWriter
from base_model import BaseModel
import os
from chunk_progress import get_chr_chunk_progress, get_gene_chunk_progress 

class EpactsModel(BaseModel):

    def __init__(self, working_directory, app_config, cmd=""):
        BaseModel.__init__(self, working_directory, app_config) 
        self.cmd = cmd
        self.cores_per_job = 56

    def get_opts(self, model, geno):
        opts = []
        if model.get("response_invnorm", False):
            opts.append("--inv-norm")
        return opts 

    def get_ped_writer(self, model_spec, geno, pheno):
        ped_writer = PedWriter(pheno.get_pheno_reader(), \
            model_spec["response"], model_spec.get("covariates",[])) 
        if "genopheno" in model_spec and len(model_spec["genopheno"])>0:
            ped_writer.merge_covar(geno.get_pheno_reader(), \
                model_spec["genopheno"])
        return ped_writer

    def get_analysis_commands(self, model_spec, geno, ped):
        cmd = "{} {}".format(self.app_config.get("ANALYSIS_BINARY", "epacts"), self.cmd) + \
            " --vcf {}".format(geno.get_vcf_path(1)) + \
            " --ped {}".format(ped.get("path")) +  \
            " --field GT" + \
            " --sepchr" + \
            " --ref {}".format(geno.get_build_ref_path())+ \
            " --out ./output --run {}".format(self.cores_per_job) 
        for resp in ped.get("response"):
            cmd += " --pheno {}".format(resp)
        for covar in ped.get("covars"):
            cmd += " --cov {}".format(covar)
        cmd += " " + " ".join(self.get_opts(model_spec, geno)) 
        cmd += " 2> ./err.log 1> ./out.log"
        return [cmd, "EXIT_STATUS=$?", "echo $EXIT_STATUS > ./exit_status.txt"]

    def get_postprocessing_commands(self, geno):
        cmds = []
        cmds.append("if [ $EXIT_STATUS == 0 ]; then")
        if self.cmd == "group":
            cmds.append("if [ -e output.epacts -a ! -e output.epacts.gz ]; then\n" + \
                "  awk 'NR<2{print;next}{print| \"sort -V -k1,1 -k2g,3\"}' output.epacts | " + \
                "{} -c > output.epacts.gz\n".format(self.app_config.get("BGZIP_BINARY", "bgzip")) + \
                " {} -p bed output.epacts.gz\n".format(self.app_config.get("TABIX_BINARY", "tabix")) + \
                "fi")
        if self.app_config.get("MANHATTAN_BINARY"):
            cmd  = "{} ./output.epacts.gz ./manhattan.json".format(self.app_config.get("MANHATTAN_BINARY", ""))
            cmds.append(cmd)
        if self.app_config.get("TOPHITS_BINARY"):
            cmd = "{} ./output.epacts.gz ./qq.json".format(self.app_config.get("QQPLOT_BINARY", ""))
            cmds.append(cmd)
        if self.app_config.get("TOPHITS_BINARY"):
            cmd =  "{} ./output.epacts.top5000 ./tophits.json".format(self.app_config.get("TOPHITS_BINARY"))
            if self.cmd == "group":
                cmd += " --window 0"
            elif geno.get_build_nearest_gene_path():
                cmd += " --gene {}".format(geno.get_build_nearest_gene_path())
            cmds.append(cmd)
        cmds.append("fi")
        cmds.append("exit $EXIT_STATUS")
        return cmds 

    def write_ped_file(self, ped_file_path, model_spec, geno=None, pheno=None):
        geno = self.get_geno(model_spec, geno)
        pheno = self.get_pheno(model_spec, pheno)
       
        try:
            ped_writer = self.get_ped_writer(model_spec, geno, pheno) 
            with open(ped_file_path, "w") as pedfile:
                ped_writer.write_to_file(pedfile)
            return {
                "response": ped_writer.get_response_headers(),
                "covars": ped_writer.get_covar_headers(),
                "path": ped_file_path
            }
        except Exception as e:
            raise Exception("Failed to create ped file ({})".format(e))

    def prepare_job(self, model_spec):
        geno = self.get_geno(model_spec)
        pheno = self.get_pheno(model_spec)

        ped = self.write_ped_file(self.relative_path("pheno.ped"), model_spec, geno, pheno)
        cmds =  self.get_analysis_commands(model_spec, geno, ped)
        cmds += self.get_postprocessing_commands(geno)

        return {"commands": cmds}

    def get_progress(self):
        output_file_glob = self.relative_path("output.*.epacts")
        if os.path.isfile(self.relative_path("output.1.R")):
            resp = get_gene_chunk_progress(output_file_glob,
                self.relative_path("output.*.R"))
        else:
            fre = r'output.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.epacts$'
            resp = get_chr_chunk_progress(output_file_glob, fre)
        return resp

        
class LMEpactsModel(EpactsModel):
    model_code = "lm"
    model_name = "Linear Wald Test"
    model_desc = "A simple linear model"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "single")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["--test q.linear",
            "--unit 500000", 
            "--min-maf 0.001" ]
        return opts

class LMMEpactsModel(EpactsModel):
    model_code = "lmm"
    model_name = "Linear Mixed Model"
    model_desc = "Adjust for potential relatedness using kinship matrix"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "single")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["--test q.emmax",
            "--kin {}".format(geno.get_kinship_path()), 
            "--unit 500000",
            "--min-maf 0.001"] 
        return opts

class SkatOEpactsModel(EpactsModel):
    model_code = "skato"
    model_name = "SKAT-O Test"
    model_desc = "Adaptive burden test"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test skat",
            "--skat-o",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--unit 500",
            "--max-maf 0.05"] 
        return opts
    
class MMSkatOEpactsModel(EpactsModel):
    model_code = "mmskato"
    model_name = "Mixed Model SKAT-O Test"
    model_desc = "Adaptive burden test that adjusts for potential relatedness using kinship matrix"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test mmskat",
            "--skat-o",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 500",
            "--max-maf 0.05"] 
        return opts

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

def get_chr_chunk_progress(output_file_glob):
    files = glob.glob(output_file_glob)
    now = time.mktime(time.localtime())
    if len(files):
        chunks = []
        p = re.compile(r'output.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.epacts$')
        for file in files:
            m = p.search(file)
            chunk = dict(m.groupdict())
            chunk['chrom'] =  chunk['chr']
            if not chunk['chrom'].startswith("chr"):
                chunk['chrom'] = "chr" + chunk['chrom']
            chunk['start'] = int(chunk['start'])
            chunk['stop'] = int(chunk['stop'])
            chunk['modified'] = time.mktime(time.localtime(os.path.getmtime(file)))
            chunks.append(chunk)
        
        result = collapse_chunk_bins(bin_chunks_by_chr_and_age(chunks, now))
        return {"data": result, "header": {"format": "ideogram"}}

