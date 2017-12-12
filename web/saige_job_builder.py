from base_model import BaseModel
from ped_writer import PedWriter
import os
import re


class SaigeModel(BaseModel):
    def __init__(self, working_directory="./", app_config=None):
        BaseModel.__init__(self, working_directory, app_config) 
        self.cores_per_job = 56

    def get_opts(self, model_spec, geno):
        opts = []
        if model_spec.get("response_invnorm", False):
            opts.append("INVNORM=TRUE")
        return opts 

    def get_ped_writer(self, model, geno, pheno):
        ped_writer = PedWriter(pheno.get_pheno_reader(), \
            model["response"], model.get("covariates",[])) 
        if "genopheno" in model and len(model["genopheno"])>0:
            ped_writer.merge_covar(geno.get_pheno_reader(), \
                model["genopheno"])
        return ped_writer

    def get_analysis_commands(self, model_spec, geno, ped):
        cmd = "{}".format(self.app_config.get("SAIGE_BINARY", "saige")) + \
            " -j{} ".format(self.cores_per_job) + \
            " THREADS={}".format(self.cores_per_job) + \
            " VCFFILE={}".format(geno.get_vcf_path(1)) + \
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
        return {}

        
class LinearSaigeModel(SaigeModel):
    model_code = "saige-qt"
    model_name = "Saige Linear Mixed Model"
    model_desc = "Fast linear mixed model with kinship adjustment"

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

    def __init__(self, working_directory, config):
        SaigeModel.__init__(self, working_directory, config)

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["RESPONSETYPE=binary"] 
        return opts

