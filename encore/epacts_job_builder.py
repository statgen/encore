from .ped_writer import PedWriter
from .base_model import BaseModel
import os
from .chunk_progress import get_chr_chunk_progress, get_gene_chunk_progress 

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
        if model.get("variant_filter", False):
            vf = model.get("variant_filter")
            if vf == "min-maf-001":
                opts.append("--min-maf 0.001")
            elif vf == "min-mac-20":
                opts.append("--min-mac 20")
            elif vf == "max-maf-05":
                opts.append("--max-maf 0.05")
            else:
                raise Exception("Unrecognized variant filter ({})".format(vf))
        if model.get("region", None):
            region = model.get("region") + ":0"
            opts.append("--region {}".format(region))
        return opts 

    def get_ped_writer(self, model_spec, geno, pheno):
        ped_writer = PedWriter(pheno.get_pheno_reader(), \
            model_spec["response"], model_spec.get("covariates",[])) 
        if "genopheno" in model_spec and len(model_spec["genopheno"])>0:
            ped_writer.merge_covar(geno.get_pheno_reader(), \
                model_spec["genopheno"])
        return ped_writer

    def get_analysis_commands(self, model_spec, geno, ped):
        pipeline = model_spec.get("pipeline_version", "epacts-3.3")
        binary = self.app_config.get("EPACTS_BINARY", None)
        if isinstance(binary, dict):
            binary = binary.get(pipeline, None)
        if not binary:
            raise Exception("Unable to find EPACTS binary (pipeline: {})".format(pipeline))
        infile = geno.get_sav_path(1, must_exist=True)
        if not infile:
            infile = geno.get_vcf_path(1, must_exist=True)
        if not infile:
            raise Exception("Unable to find genotype input (genotype: {})".format(geno.geno_id))
        cmd = "{} {}".format(binary, self.cmd) + \
            " --vcf {}".format(infile) + \
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

    def validate_model_spec(self, model_spec):
        if not "pipeline_version" in model_spec:
            if "EPACTS_VERSION" in self.app_config:
                model_spec["pipeline_version"] = self.app_config["EPACTS_VERSION"]
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

        resp = model_spec.get("response", None)
        if not resp:
            raise Exception("Missing model response")

        if isinstance(resp, dict):
            resp_name = resp.get("name", None)
        else:
            resp_name = resp

        pheno = self.get_pheno(model_spec)
        resp_class = pheno.get_column_class(resp_name)
        if resp_class == "binary":
            if not isinstance(resp, dict):
                resp = {"name": resp}
            resp_event = resp.get("event", None)
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
        elif resp_class != "numeric":
            raise Exception("Response must be binary or numeric, found: {}".format(resp_class))
        
class LMEpactsModel(EpactsModel):
    model_code = "lm"
    model_name = "Linear Wald Test"
    model_desc = "A simple linear model"
    depends = ["vcf|sav"]
    filters = [("min-maf-001", "MAF > 0.1%"), ("min-mac-20", "MAC > 20")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "single", "lm")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["--test q.linear",
            "--unit 500000"]
        return opts

class LMMEpactsModel(EpactsModel):
    model_code = "lmm"
    model_name = "Linear Mixed Model"
    model_desc = "Adjust for potential relatedness using kinship matrix"
    depends = ["vcf|sav", "kinship"]
    filters = [("min-maf-001", "MAF > 0.1%"), ("min-mac-20","MAC > 20")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "single", "lmm")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["--test q.emmax",
            "--kin {}".format(geno.get_kinship_path()), 
            "--unit 500000"]
        return opts

class SkatOEpactsModel(EpactsModel):
    model_code = "skato"
    model_name = "SKAT-O Test"
    model_desc = "Adaptive burden test"
    depends = ["vcf|sav", "group_nonsyn"]
    filters = [("max-maf-05", "MAF < 5%")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "skato")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test skat",
            "--skat-o",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--unit 350"]
        return opts
    
class MMSkatOEpactsModel(EpactsModel):
    model_code = "mmskato"
    model_name = "Mixed Model SKAT-O Test"
    model_desc = "Adaptive burden test that adjusts for potential relatedness using kinship matrix"
    depends = ["vcf|sav", "group_nonsyn", "kinship"]
    filters = [("max-maf-05", "MAF < 5%")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmskato")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test mmskat",
            "--skat-o",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 350"]
        return opts

class MMSkatEpactsModel(EpactsModel):
    model_code = "mmskat"
    model_name = "Mixed Model SKAT Test"
    model_desc = "Burden test that adjusts for potential relatedness using kinship matrix"
    depends = ["vcf|sav", "group_nonsyn", "kinship"]
    filters = [("max-maf-05", "MAF < 5%")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmskat")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test mmskat",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 300"]
        return opts

class MMVTEpactsModel(EpactsModel):
    model_code = "mmVT"
    model_name = "Mixed Model Variable-Threshold Test"
    model_desc = "Variable-threshold burden test that adjusts for potential relatedness using kinship matrix"
    depends = ["vcf|sav", "group_nonsyn", "kinship"]
    filters = [("max-maf-05", "MAF < 5%")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmVT")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test emmaxVT",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 300"]
        return opts

class MMCMCEpactsModel(EpactsModel):
    model_code = "mmCMC"
    model_name = "Mixed Model Collapsing Burden Test"
    model_desc = "Collapsing burden test that adjusts for potential relatedness using kinship matrix"
    depends = ["vcf|sav", "group_nonsyn", "kinship"]
    filters = [("max-maf-05", "MAF < 5%")]

    def __init__(self, working_directory, app_config):
        EpactsModel.__init__(self, working_directory, app_config, "group", "mmCMC")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test emmaxCMC",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--kin {}".format(geno.get_kinship_path()),
            "--unit 300"]
        return opts
