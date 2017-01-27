from genotype import Genotype
from phenotype import Phenotype 
from ped_writer import PedWriter
import os
import subprocess

class EpactsModel(object):
    __models = []

    def __init__(self, config, cmd=""):
        self.cmd = cmd
        self.config = config
        self.cores_per_job = 56

    def get_opts(self, model, geno):
        opts = []
        if model.get("response_invnorm", False):
            opts.append("--inv-norm")
        return opts 

    def get_ped_writer(self, model, geno, pheno):
        ped_writer = PedWriter(pheno.get_pheno_reader(), \
            model["response"], model.get("covariates",[])) 
        if "genopheno" in model and len(model["genopheno"])>0:
            ped_writer.merge_covar(geno.get_pheno_reader(), \
                model["genopheno"])
        return ped_writer

    def get_analysis_command(self, model, geno, ped_path, ped_writer):
        cmd = "{} {}".format(self.config.get("ANALYSIS_BINARY", "epacts"), self.cmd) + \
            " --vcf {}".format(geno.get_vcf_path(1)) + \
            " --ped {}".format(ped_path) +  \
            " --field GT" + \
            " --sepchr" + \
            " --out ./output --run {}".format(self.cores_per_job)
        for resp in ped_writer.get_response_headers():
            cmd += " --pheno {}".format(resp)
        for covar in ped_writer.get_covar_headers():
            cmd += " --cov {}".format(covar)
        cmd += " " + " ".join(self.get_opts(model, geno)) 
        return cmd

    def get_postprocessing_command(self):
        cmds = []
        if self.cmd == "group":
            cmds.append("if [ -e output.epacts -a ! -e output.epacts.gz ]; then\n" + \
                "  awk 'NR<2{print;next}{print| \"sort -g -k1,1 -k2g,3\"}' output.epacts | " + \
                "{} -c > output.epacts.gz\n".format(self.config.get("BGZIP_BINARY", "bgzip")) + \
                " {} -p bed output.epacts.gz\n".format(self.config.get("TABIX_BINARY", "tabix")) + \
                "fi")
        if self.config.get("MANHATTAN_BINARY"):
            cmd  = "{} ./output.epacts.gz ./manhattan.json".format(self.config.get("MANHATTAN_BINARY", ""))
            cmds.append(cmd)
        if self.config.get("TOPHITS_BINARY"):
            cmd = "{} ./output.epacts.gz ./qq.json".format(self.config.get("QQPLOT_BINARY", ""))
            cmds.append(cmd)
        if self.config.get("TOPHITS_BINARY"):
            cmd =  "{} ./output.epacts.top5000 ./tophits.json".format(self.config.get("TOPHITS_BINARY"))
            if self.cmd == "group":
                cmd += " --window 0"
            elif self.config.get("NEAREST_GENE_BED"):
                cmd += " --gene " + self.config.get("NEAREST_GENE_BED") 
            cmds.append(cmd)
        return "\n".join(cmds) 

    @staticmethod
    def get(model, config):
        for m in EpactsModel.__models:
            if m.model_code == model:
                return m(config)
        raise ValueError('Unrecognized model type: %s' % (model_type,))

    @staticmethod
    def list():
        def desc(x):
            return {"code": x.model_code, "name": x.model_name, "description": x.model_desc}
        return [ desc(m) for m in EpactsModel.__models]

    @staticmethod
    def register(m):
        EpactsModel.__models.append(m)
        
class LMEpactsModel(EpactsModel):
    model_code = "lm"
    model_name = "Linear Wald Test"
    model_desc = "A simple linear model"

    def __init__(self, config):
        EpactsModel.__init__(self, config, "single")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["--test q.linear",
            "--unit 500000", 
            "--min-maf 0.001" ]
        return opts
EpactsModel.register(LMEpactsModel)

class LMMEpactsModel(EpactsModel):
    model_code = "lmm"
    model_name = "Linear Mixed Model"
    model_desc = "Adjust for potential relatedness using kinship matrix"

    def __init__(self, config):
        EpactsModel.__init__(self, config, "single")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        opts += ["--test q.emmax",
            "--kin {}".format(geno.get_kinship_path()), 
            "--unit 500000",
            "--min-maf 0.001"] 
        return opts
EpactsModel.register(LMMEpactsModel)


class SkatOEpactsModel(EpactsModel):
    model_code = "skato"
    model_name = "SKAT-O Test"
    model_desc = "Adaptive burden test"

    def __init__(self, config):
        EpactsModel.__init__(self, config, "group")

    def get_opts(self, model, geno):
        opts = super(self.__class__, self).get_opts(model, geno) 
        group = model.get("group", "nonsyn")
        opts += ["--test skat",
            "--skat-o",
            "--groupf {}".format(geno.get_groups_path(group)),
            "--unit 500",
            "--max-maf 0.05"] 
        return opts
EpactsModel.register(SkatOEpactsModel)
    
class MMSkatOEpactsModel(EpactsModel):
    model_code = "mmskato"
    model_name = "Mixed Model SKAT-O Test"
    model_desc = "Adaptive burden test that adjusts for potential relatedness using kinship matrix"

    def __init__(self, config):
        EpactsModel.__init__(self, config, "group")

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
EpactsModel.register(MMSkatOEpactsModel)

class SlurmEpactsJob:

    def __init__(self, job_id, job_directory, config = None):
        self.job_id = job_id
        self.job_directory = job_directory
        if config:
            self.config = config 
        else:
            self.config = dict()

    def create_sbatch_header(self, epm):
       return "#!/bin/bash\n" + \
           "#SBATCH --partition=encore\n" + \
           "#SBATCH --job-name=gasp_{}\n".format(self.job_id)  + \
           "#SBATCH --cpus-per-task={}\n".format(epm.cores_per_job)  + \
           "#SBATCH --workdir={}\n".format(self.job_directory) + \
           "#SBATCH --mem-per-cpu=6500\n" + \
           "#SBATCH --time=14-0\n" + \
           "#SBATCH --nodes=1\n"

    def create_analysis_command(self, epm, job_desc, geno=None, pheno=None):
        if geno is None:
            if "genotype" in job_desc:
                geno = Genotype.get(job_desc["genotype"], self.config)
            else:
                raise Exception("No genotype information in job")
        
        if pheno is None:
            if "phenotype" in job_desc:
                pheno = Phenotype.get(job_desc["phenotype"], self.config)
            else:
                raise Exception("No phenotype information in job")

        ped_writer = epm.get_ped_writer(job_desc, geno, pheno) 
        ped_path = self.relative_path("pheno.ped")
        with open(ped_path,"w") as pedfile:
            ped_writer.write_to_file(pedfile)

        return epm.get_analysis_command(job_desc, geno, ped_path, ped_writer)

    def create_reproducible_command(self, job_desc):
        epm = EpactsModel.get(job_desc.get("type", None), {})
        geno = Genotype(None)
    
        if "phenotype" in job_desc:
            pheno = Phenotype.get(job_desc["phenotype"], self.config)
        else:
            raise Exception("No phenotype information in job")

        ped_writer = epm.get_ped_writer(job_desc, geno, pheno) 

        return epm.get_analysis_command(job_desc, geno, ped_path="pheno.ped", ped_writer=ped_writer)

    def create_postprocessing_command(self, epm):
        cmd =  "if [ $EXIT_STATUS == 0 ]; then\n" 
        cmd += epm.get_postprocessing_command() 
        cmd += "\nfi\n"
        return cmd

    def create_launch_script(self, job_desc):
        model_type = job_desc.get("type", None)
        epm = EpactsModel.get(model_type, self.config)

        cmd = self.create_sbatch_header(epm) + "\n"
        cmd += "{} 2> ./err.log 1> ./out.log\n".format(self.create_analysis_command(epm, job_desc))
        cmd += "EXIT_STATUS=$?\n"
        cmd += self.create_postprocessing_command(epm) + "\n" 
        cmd += "echo $EXIT_STATUS > ./exit_status.txt\n"
        cmd += "exit $EXIT_STATUS\n"
        return cmd

    def submit_job(self, job_desc):
        sbatch = self.config.get("QUEUE_JOB_BINARY", "sbatch")
        batch_script_path = self.relative_path("batch_script.sh")
        batch_output_path = self.relative_path("batch_script_output.txt")
        with open(batch_script_path, "w") as f:
            f.write(self.create_launch_script(job_desc))
        with open(batch_output_path, "w") as f:
            try:
                subprocess.call([sbatch, batch_script_path], stdout=f)
            except subprocess.CalledProcessError:
                raise Exception("Could not queue job") 
            except OSError:
                raise Exception("Could not find sbatch")
        return True
 
    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.job_directory, *args))

    @staticmethod
    def available_models():
        return EpactsModel.list()
