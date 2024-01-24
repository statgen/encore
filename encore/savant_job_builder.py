from .base_model import BaseModel
import os
from .chunk_progress import get_chr_chunk_progress
import yaml


class savantModel(BaseModel):
    filters = [("min-mac-20", "MAC > 20"),
               ("min-maf-001", "MAF > 0.1%"),
               ("min-maf-001-mac-20","MAF > 0.1% AND MAC > 20")]

    def __init__(self, working_directory="./", app_config=None):
        BaseModel.__init__(self, working_directory, app_config)
        self.cores_per_job = 56

    def returnContigs(self,region):
        contigVal={}
        contigDict = {
            "chr1":248956422,
            "chr2":242193529,
            "chr3":198295559,
            "chr4":190214555,
            "chr5":181538259,
            "chr6":170805979,
            "chr7":159345973,
            "chr8":145138636,
            "chr9":138394717,
            "chr10":133797422,
            "chr11":135086622,
            "chr12":133275309,
            "chr13":114364328,
            "chr14":107043718,
            "chr15":101991189,
            "chr16":90338345,
            "chr17":83257441,
            "chr18":80373285,
            "chr19":58617616,
            "chr20":64444167,
            "chr21":46709983,
            "chr22":50818468,
            "chrX":156040895
        }

        if(region == "all"):
            contigVal= contigDict
        else:
            regval = contigDict.get(region)
            contigVal[region]=regval


        return contigVal

    def get_opts(self, model, geno):
        opts = {}
        if model.get("response_invnorm", False):
            opts['inv_norm']= 'true'
        if model.get("variant_filter", False):
            vf = model.get("variant_filter")
            if vf == "min-maf-001-mac-20":
                opts['min_mac']=20
                opts['min_maf']= 0.001
            elif vf == "min-maf-001":
                #opts.append("STEP2OPT='--minMAF 0.001 --IsOutputAFinCaseCtrl=FALSE'")
                opts['min_maf']= 0.001
            elif vf == "min-mac-20":
                opts['min_mac']= 20

            else:
                raise Exception("Unrecognized variant filter ({})".format(vf))

        region_value = model.get("region", None)

        if region_value is None:
            contigval = self.returnContigs("all")
            opts['contigs']=contigval
        else:
            region = model.get("region")
            print("region",region)
            if region.startswith("CHR"):
                region = region[3:]
            #opts.append("region=".format(region))
            contigval = self.returnContigs(region)
            opts['contigs']=contigval
            opts['region_size']=5000000
        opts['region_size']=100000
        # elif geno.get_chromosomes():
        #     opts.append("CHRS='{}'".format(geno.get_chromosomes()))
        return opts

    def get_analysis_commands(self, model_spec, geno, pheno, ped):
        pipeline = self.app_config["SAVANT_SIF_FILE"][0]
        if "SAVANT_BINARY" in self.app_config:
            binary = self.app_config["SAVANT_BINARY"]
        if isinstance(binary, tuple):
            binary = binary[0]
        if not binary:
            raise Exception("Unable to find Savant sif file file  (pipeline: {})".format(pipeline))
        cmd = "singularity exec -B /net/encore1/savant:/net/encore1/savant:ro -B /net/encore1/encoredata:/net/encore1/encoredata {} ".format(pipeline) + \
              " snakemake --snakefile {}".format(binary)+ \
              " -j ${SLURM_CPUS_PER_TASK}"

        optlist = self.get_opts(model_spec, geno)
        optlist["input_vcf_expression"]= geno.get_sav_path(1).replace("chr1", "{chrom}")
        optlist["pheno_file"]= ped.get("path")

        for resp in ped.get("response"):
            optlist['response']=resp
        covars = ped.get("covars")
        if len(covars)>0:
            optlist['covariates']=covars
        confilepath = self.relative_path("config.yml")
        with open(confilepath, 'w') as file:
            documents = yaml.dump(optlist, file)
        return [cmd]

    def get_postprocessing_commands(self, geno, result_file="./results.txt.gz"):
        cmds = []
        if self.app_config.get("VENV_PATH"):
            cmd  = "{}".format(self.app_config.get("VENV_PATH")[0])
            cmds.append(cmd)
        cmds.append("zcat -f {} | ".format(result_file) + \
                    'awk -F"\\t" \'BEGIN {OFS="\\t"} NR==1 {for (i=1; i<=NF; ++i) {if($i=="pvalue") pcol=i}; if (pcol<1) exit 1; print} ' + \
                    '($pcol < 0.001) {print}\' | ' + \
                    "{} -c > output.filtered.001.gz".format(self.app_config.get("BGZIP_BINARY", "bgzip")))
        if self.app_config.get("TABIX_BINARY"):
            cmd  = " {} -s1 -b2 -e2  results.txt.gz".format(self.app_config.get("TABIX_BINARY", "tabix"))
            cmds.append(cmd)
        if self.app_config.get("MANHATTAN_BINARY"):
            cmd  = "{} {} ./manhattan.json".format(self.app_config.get("MANHATTAN_BINARY", ""), result_file)
            cmds.append(cmd)
        if self.app_config.get("TOPHITS_BINARY"):
            cmd = "{} {} ./qq.json".format(self.app_config.get("QQPLOT_BINARY", ""), result_file)
            cmds.append(cmd)
        if self.app_config.get("TOPHITS_BINARY"):
            cmd =  "{} {} ./tophits.json".format(self.app_config.get("TOPHITS_BINARY"), result_file)
            if geno.get_build_nearest_gene_path():
                cmd += " --gene {}".format(geno.get_build_nearest_gene_path())
            cmds.append(cmd)
        return cmds

    def write_ped_file(self, ped_file_path, model_spec, geno=None, pheno=None):
        geno = self.get_geno(model_spec, geno)
        pheno = self.get_pheno(model_spec, pheno)

        try:
            ped_writer = self.get_ped_writer(model_spec, geno, pheno)
            with open(ped_file_path, "w") as pedfile:
                ped_writer.write_to_file(pedfile, comment_header=False)
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

        #print("prepare jobs")
        #print("write_ped_file",self.relative_path("pheno.ped"))

        ped = self.write_ped_file(self.relative_path("pheno.ped"), model_spec, geno, pheno)
        cmds =  self.if_exit_success(
            self.get_analysis_commands(model_spec, geno, pheno, ped),
            self.get_postprocessing_commands(geno))
        return {"commands": cmds}

    def get_progress(self):
        output_file_glob = self.relative_path("tmp/out.savant-lm*.tsv.tmp")
        print(output_file_glob)
        #fre = r'step2\.bin\.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.txt$'
        fre= r'out\.savant-lm\.(?P<chr>\w+)\_(?P<start>\d+)\_(?P<stop>\d+)\.tsv.tmp$'
        print("inside get progress")
        resp = get_chr_chunk_progress(output_file_glob, fre)
        return resp


    def get_progress(self):
        output_file_glob = self.relative_path("tmp/out.savant-lm*.tsv")
        print(output_file_glob)
        #fre = r'step2\.bin\.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.txt$'
        fre= r'out\.savant-lm\.(?P<chr>\w+)\_(?P<start>\d+)\_(?P<stop>\d+)\.tsv$'
        print("inside get progress")
        resp = get_chr_chunk_progress(output_file_glob, fre)
        return resp


    def validate_model_spec(self, model_spec):
        if not "pipeline_version" in model_spec:
            if "SAVANT_VERSION" in self.app_config:
                model_spec["pipeline_version"] = self.app_config["SAVANT_VERSION"]
        if "variant_filter" in model_spec:
            if hasattr(self, "filters"):
                vf = model_spec["variant_filter"]
                if not any([vf == x[0] for x in self.filters]):
                    raise Exception("Did not recognize variant filter ({})".format(vf))
            else:
                raise Exception("No variant filters defined but one was requested ({})".format(e))
        else:
            if hasattr(self, "filters"):
                model_spec["variant_filter"] = self.filters[0][0]

    def get_failure_reason(self):
        log_file_path = self.relative_path("saige.log")
        if not os.path.isfile(log_file_path):
            return None
        with open(log_file_path, 'rt') as f:
            for line in f:
                if "matrix is singular" in line:
                    return "Matrix is singular or not positive definite"
                elif "parameter estimate is 0" in line:
                    return "The first variance component parameter estimate is 0"
                elif "variance of the phenotype is much smaller" in line:
                    return ("Variance of the phenotype is much smaller than 1. "
                            "Please consider using inverse normalized response")
        return None

class LinearSavantModel(savantModel):
    model_code = "savant-lm"
    model_name = "SAVANT Single variant association analysis"
    model_desc = "Fast Simple Linear Model"
    depends = ["sav"]

    def __init__(self, working_directory, config):
        savantModel.__init__(self, working_directory, config)

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno)
        #opts += ["RESPONSETYPE=quantitative"]
        opts["model"]=self.model_code
        return opts

class BinarySavantModel(savantModel):
    model_code = "savant-bin"
    model_name = "Savant Logistic Regression Model"
    model_desc = "Fast Binary regression model"
    depends = ["sav", "snps"]
    response_class = "binary"

    def __init__(self, working_directory, config):
        savantModel.__init__(self, working_directory, config)

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno)
        opts["logit"]='true'
        opts["model"]='savant-lm'
        return opts

    def validate_model_spec(self, model_spec):
        super(self.__class__, self).validate_model_spec(model_spec)
        resp = model_spec.get("response", None)
        if not resp:
            raise Exception("Missing model response")

        if not isinstance(resp, dict):
            resp = {"name": resp}
        resp_name = resp.get("name", None)
        resp_event = resp.get("event", None)

        pheno = self.get_pheno(model_spec)
        levels = pheno.get_column_levels(resp_name)
        if not levels:
            raise Exception(("Variable {} does not appear to be a binary trait "
                             "(No levels found)").format(resp_name))
        if len(levels) != 2:
            raise Exception("Response must have exactly 2 levels, found {}: {}".
                            format(len(levels), ",".join(levels)))
        if resp_event:
            if not resp_event in levels:
                raise Exception(("Requested event level does not match data. "
                                 "Requested: {}; Data: {}").format(resp_event, ",".join(levels)))
        else:
            resp_event = levels[1]

        resp["event"] = resp_event
        model_spec["response"] = resp
        model_spec["response_desc"] = "Pr({} = {})".format(resp_name, resp_event)


