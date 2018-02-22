from ped_writer import PedWriter
from base_model import BaseModel
import os
from chunk_progress import get_chr_chunk_progress, get_gene_chunk_progress 

class EpactsModel(BaseModel):

    def __init__(self, working_directory, app_config, cmd="", code=""):
        BaseModel.__init__(self, working_directory, app_config) 
        self.cmd = cmd
        self.code = code
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
        else:
            if self.code == "lmm":
                cmds.append("zcat -f output.epacts.gz | " + \
                    'awk -F"\\t" \'NR==1 || ($9 > 0 && $11 < 0.001) {OFS="\\t"; print }\' | ' + \
                    "{} -c > output.filtered.001.gz".format(self.app_config.get("BGZIP_BINARY", "bgzip")))
            elif self.code == "lm":
                cmds.append("zcat -f output.epacts.gz | " + \
                    'awk -F"\\t" \'NR==1 || ($8 > 0 && $9 < 0.001) {OFS="\\t"; print }\' | ' + \
                    "{} -c > output.filtered.001.gz".format(self.app_config.get("BGZIP_BINARY", "bgzip")))
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
        elif os.path.isfile(self.relative_path("output.1.grp")):
            resp = get_gene_chunk_progress(output_file_glob,
                self.relative_path("output.*.grp"), min_done_age=30)
        else:
            fre = r'output.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.epacts$'
            resp = get_chr_chunk_progress(output_file_glob, fre)
        return resp

        
class LMEpactsModel(EpactsModel):
    model_code = "lm"
    model_name = "Linear Wald Test"
    model_desc = "A simple linear model"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "single", "lm")

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
        EpactsModel.__init__(self, working_directory, app_config, "single", "lmm")

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
        EpactsModel.__init__(self, working_directory, app_config, "group", "skato")

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
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmskato")

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

class MMSkatEpactsModel(EpactsModel):
    model_code = "mmskat"
    model_name = "Mixed Model SKAT Test"
    model_desc = "Burden test that adjusts for potential relatedness using kinship matrix"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmskat")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test mmskat",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 300",
            "--max-maf 0.05"] 
        return opts

class MMVTEpactsModel(EpactsModel):
    model_code = "mmVT"
    model_name = "Mixed Model Variable-Threshold Test"
    model_desc = "Variable-threshold burden test that adjusts for potential relatedness using kinship matrix"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmVT")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test emmaxVT",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 300",
            "--max-maf 0.05"] 
        return opts

class MMCMCEpactsModel(EpactsModel):
    model_code = "mmCMC"
    model_name = "Mixed Model Collapsing Burden Test"
    model_desc = "Collapsing burden test that adjusts for potential relatedness using kinship matrix"

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmCMC")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test emmaxCMC",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 300",
            "--max-maf 0.05"] 
        return opts
