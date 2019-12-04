from .base_model import BaseModel
from .ped_writer import PedWriter
import os
from .chunk_progress import get_chr_chunk_progress


class SaigeModel(BaseModel):
    filters = [("min-mac-20", "MAC > 20"),
        ("min-maf-001", "MAF > 0.1%"),
        ("min-maf-001-mac-20","MAF > 0.1% AND MAC > 20")]

    def __init__(self, working_directory="./", app_config=None):
        BaseModel.__init__(self, working_directory, app_config) 
        self.cores_per_job = 56

    def get_opts(self, model, geno):
        opts = []
        if model.get("response_invnorm", False):
            opts.append("INVNORM=TRUE")
        if model.get("variant_filter", False):
            vf = model.get("variant_filter")
            if vf == "min-maf-001-mac-20":
                opts.append("STEP2OPT='--minMAF 0.001 --minMAC 20 --IsOutputAFinCaseCtrl=FALSE'")
            elif vf == "min-maf-001":
                opts.append("STEP2OPT='--minMAF 0.001 --IsOutputAFinCaseCtrl=FALSE'")
            elif vf == "min-mac-20":
                opts.append("STEP2OPT='--minMAC 20 --IsOutputAFinCaseCtrl=FALSE'")
            else:
                raise Exception("Unrecognized variant filter ({})".format(vf))
        if model.get("region", None):
            region = model.get("region").upper()
            if region.startswith("CHR"):
                region = region[3:]
            opts.append("CHRS={}".format(region))
            opts.append("BINSIZE={}".format(100000))
        return opts 

    def get_ped_writer(self, model, geno, pheno):
        ped_writer = PedWriter(pheno.get_pheno_reader(), \
            model["response"], model.get("covariates",[])) 
        if "genopheno" in model and len(model["genopheno"])>0:
            ped_writer.merge_covar(geno.get_pheno_reader(), \
                model["genopheno"])
        return ped_writer

    def get_analysis_commands(self, model_spec, geno, ped):
        pipeline = model_spec.get("pipeline_version", "saige-0.26")
        binary = self.app_config.get("SAIGE_BINARY", None)
        if isinstance(binary, dict):
            binary = binary.get(pipeline, None)
        if not binary:
            raise Exception("Unable to find SAIGE binary (pipeline: {})".format(pipeline))
        cmd = "{}".format(binary) + \
            " -j{} ".format(self.cores_per_job) + \
            " THREADS={}".format(self.cores_per_job) + \
            " SAVFILE={}".format(geno.get_sav_path(1)) + \
            " PHENOFILE={}".format(ped.get("path")) +  \
            " REFFILE={}".format(geno.get_build_ref_path())+ \
            " PLINKFILE={}".format(geno.get_pca_genotypes_path()) + \
            " SAMPLEFILE={}".format(geno.get_samples_path()) 
        for resp in ped.get("response"):
            cmd += " RESPONSE={}".format(resp)
        covars = ped.get("covars")
        if len(covars)>0:
            cmd += " COVAR={}".format(",".join(covars))
        cmd += " " + " ".join(self.get_opts(model_spec, geno)) 
        return [cmd]

    def get_postprocessing_commands(self, geno, result_file="./results.txt.gz"):
        cmds = []
        cmds.append("zcat -f {} | ".format(result_file) + \
            'awk -F"\\t" \'BEGIN {OFS="\\t"} NR==1 {for (i=1; i<=NF; ++i) {if($i=="p.value") pcol=i; if($i=="N") ncol=i}; if (pcol<1 || ncol<1) exit 1; print} ' + \
            '($ncol > 0 && $pcol < 0.001) {print}\' | ' + \
            "{} -c > output.filtered.001.gz".format(self.app_config.get("BGZIP_BINARY", "bgzip")))
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

        ped = self.write_ped_file(self.relative_path("pheno.ped"), model_spec, geno, pheno)
        cmds =  self.if_exit_success(
            self.get_analysis_commands(model_spec, geno, ped), 
            self.get_postprocessing_commands(geno))
        return {"commands": cmds}

    def get_progress(self):
        output_file_glob = self.relative_path("step2.bin.*.txt")
        fre = r'step2\.bin\.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.txt$'
        resp = get_chr_chunk_progress(output_file_glob, fre)
        return resp

    def validate_model_spec(self, model_spec):
        if not "pipeline_version" in model_spec:
            if "SAIGE_VERSION" in self.app_config:
                model_spec["pipeline_version"] = self.app_config["SAIGE_VERSION"]
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
        return None
        
class LinearSaigeModel(SaigeModel):
    model_code = "saige-qt"
    model_name = "Saige Linear Mixed Model"
    model_desc = "Fast linear mixed model with kinship adjustment"
    depends = ["sav", "snps"]

    def __init__(self, working_directory, config):
        SaigeModel.__init__(self, working_directory, config) 

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["RESPONSETYPE=quantitative"] 
        return opts

class BinarySaigeModel(SaigeModel):
    model_code = "saige-bin"
    model_name = "Saige Logistic Mixed Model"
    model_desc = "Fast logistic regression model with kinship adjustment"
    depends = ["sav", "snps"]
    response_class = "binary"

    def __init__(self, working_directory, config):
        SaigeModel.__init__(self, working_directory, config)

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["RESPONSETYPE=binary"] 
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

