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

        contigDict = {
            "CHR1":248956422,
            "CHR2":242193529,
            "CHR3":198295559,
            "CHR4":190214555,
            "CHR5":181538259,
            "CHR6":170805979,
            "CHR7":159345973,
            "CHR8":145138636,
            "CHR9":138394717,
            "CHR10":133797422,
            "CHR11":135086622,
            "CHR12":133275309,
            "CHR13":114364328,
            "CHR14":107043718,
            "CHR15":101991189,
            "CHR16":90338345,
            "CHR17":83257441,
            "CHR18":80373285,
            "CHR19":58617616,
            "CHR20":64444167,
            "CHR21":46709983,
            "CHR22":50818468,
            "CHRX":156040895,
            "CHRY":57227415,
            "CHRM":16569
        }

        if(region == "all"):
           contigVal= contigDict
        else:
            regval = contigDict.get(region)
            contigDict[region]=regval


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
                opts['min-maf']= 0.001
            elif vf == "min-mac-20":
                opts['min-mac']= 20

            else:
                raise Exception("Unrecognized variant filter ({})".format(vf))
        if model.get("region", None):
            region = model.get("region").upper()
            if region.startswith("CHR"):
                region = region[3:]
            contigval = self.returnContigs(region)
            opts['contigs']=contigval
            opts['region_size']=1000000
        elif geno.get_chromosomes():
            opts.append("CHRS='{}'".format(geno.get_chromosomes()))
        print("opts from the getopt")
        print(opts)
        #def  write_confif_file(self):
        # input_vcf_expression: /net/wonderland/home/lefaivej/savant/1000g-test/ALL.chr{chrom}.phase3_shapeit2_mvncall_integrated_v5.20130502.genotypes.sav
        # pheno_file: pheno_with_sex.tsv
        # model: savant-lm
        # response: Trait_1
        # min_mac: 3
        # inv_norm: true
        # covariates:
        #  - Sex
        # region_size: 1000000
        # contigs:
        #   "20": 64444167
        return opts

#get_opts
#singularity exec -B /net/wonderland:/net/wonderland:ro /net/wonderland/home/lefaivej/savant/snakemake/encore-analyses.sif snakemake --snakefile  /net/dumbo/home/snehal/SavantTestRun/Snakefile-encore -j ${SLURM_CPUS_PER_TASK}
    def get_analysis_commands(self, model_spec, geno, pheno, ped):
        pipeline = self.app_config.get("SAVANT_SIF_FILE", None)
        binary = self.app_config.get("SAVANT_BINARY", None)
        if isinstance(binary, dict):
            binary = binary.get(pipeline, None)
            print("from the if",binary)
        print("binary", binary)
        print("pipeline", pipeline)
        if not binary:
            raise Exception("Unable to find Savant sif file file  (pipeline: {})".format(pipeline))
        cmd = "singularity exec -B /net/wonderland:/net/wonderland:ro {} ".format(pipeline) + \
              " snakemake --snakefile {}".format(binary)+ \
              " -j ${SLURM_CPUS_PER_TASK}"
        print("cmd",cmd)

        optlist=self.get_opts(model_spec, geno)

        print
        optlist["input_vcf_expression"]=geno.get_sav_path(1).replace("chr1", "chr{chrom}")
        optlist["pheno_file"]=ped.get("path")
        for resp in ped.get("response"):
            optlist["RESPONSE"]=resp
        covars = ped.get("covars")
        if len(covars)>0:
             optlist["covars"]=covars
        optlist["model"]=model_spec['type']
        #     #cmd += " RESPONSE={}".format(resp)


        #input_vcf_expression: /net/wonderland/home/lefaivej/savant/1000g-test/ALL.chr{chrom}.phase3_shapeit2_mvncall_integrated_v5.20130502.genotypes.sav

        # optlist.append("input_vcf_expression: {}".format(geno.get_sav_path(1)).replace("chr1", "chr{chrom}"))
        # optlist.append("pheno_file: {}".format(ped.get("path")))
        # for resp in ped.get("response"):
        #     #cmd += " RESPONSE={}".format(resp)
        #     optlist.append("RESPONSE:{}".format(resp))
        # covars = ped.get("covars")
        # if len(covars)>0:
        #     #cmd += " COVAR={}".format(",".join(covars))
        #     optlist.append("covars:{}".format("\n -".join(covars)))
        # print("******spec*******")
        # print(model_spec['type'])
        # optlist.append("model:{}".format(model_spec['type']))



        #cmd += " " + " ".join(self.get_opts(model_spec, geno))
        print(optlist)
        confilepath = self.relative_path("config.yml")
        print(self.relative_path("config.yml"))

        dict_file = {
            "input_vcf_expression": geno.get_sav_path(1).replace("chr1", "chr{chrom}"),
            "pheno_file": ped.get("path"),
            "model": "savant-lm",
            "response": resp,
            "min_mac": "3",
            "inv_norm": "true",
            "covariates":['sex'],
            "region_size": "1000000",
            "contigs":{ "20": "64444167"},

        }

        with open(confilepath, 'w') as file:
            documents = yaml.dump(optlist, file)


        # with open(confilepath, 'w+') as fp:
        #     for item in optlist:
        #        fp.write("%s\n" % item)
        #     print('Done')

        return [cmd]

    def get_postprocessing_commands(self, geno, result_file="./results.txt.gz"):
        cmds = []
        cmds.append("tabix -s1 -b2 -e2 results.txt.gz")
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
            print("ped_file_path",ped_file_path)
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
            self.get_analysis_commands(model_spec, geno, pheno, ped),
            self.get_postprocessing_commands(geno)
        )

        return {"commands": cmds}

    def get_progress(self):
        output_file_glob = self.relative_path("step2.bin.*.txt")

        fre = r'step2\.bin\.(?P<chr>\w+)\.(?P<start>\d+)\.(?P<stop>\d+)\.txt$'
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
#singularity exec -B /net/wonderland:/net/wonderland:ro /net/wonderland/home/lefaivej/savant/snakemake/encore-analyses.sif snakemake --snakefile  /net/dumbo/home/snehal/SavantTestRun/Snakefile-encore -j ${SLURM_CPUS_PER_TASK}
class LinearSavantModel(savantModel):
    model_code = "Savant-LM"
    model_name = "SAVANT Single variant association analysis"
    model_desc = "Fast Simple Linear Model"
    depends = ["sav", "snps"]

    def __init__(self, working_directory, config):
        savantModel.__init__(self, working_directory, config)

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno)
        opts += ["RESPONSETYPE=quantitative"]
        return opts

class BinarySavantModel(savantModel):
    model_code = "Savant-bin"
    model_name = "Savant Logistic Mixed Model"
    model_desc = "Fast Binary regression model"
    depends = ["sav", "snps"]
    response_class = "binary"

    def __init__(self, working_directory, config):
        savantModel.__init__(self, working_directory, config)

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno)
        opts += ["--logit"]
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