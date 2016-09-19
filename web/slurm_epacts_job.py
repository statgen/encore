from genotype import Genotype
from phenotype import Phenotype 

class SlurmEpactsJob:

    def __init__(self, job_id, job_directory, config = None):
        self.job_id = job_id
        self.job_directory = job_directory
        if config:
            self.config = config 
        else:
            self.config = dict()

    def create_sbatch_header(self, job_desc):
       return "#!/bin/bash\n" + \
           "#SBATCH --job-name=gasp_{}\n".format(self.job_id)  + \
           "#SBATCH --cpus-per-task=48\n"  + \
           "#SBATCH --workdir={}\n".format(self.job_directory) + \
           "#SBATCH --mem-per-cpu=4000\n" + \
           "#SBATCH --time=14-0\n" + \
           "#SBATCH --nodes=1\n"

    def create_analysis_command(self, job_desc, geno=None, pheno=None):
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

        analysis_cmd = ""
        for model in job_desc["models"]:

            analysis_cmd += "{} single".format(self.config.get("ANALYSIS_BINARY", "epacts")) + \
                " --vcf {}".format(geno.getVCFPath(1)) + \
                " --ped {}".format(pheno.getRawPath()) +  \
                " --min-maf 0.001 --field GT" + \
                " --sepchr --unit 500000" + \
                " --out ./output --run 48"

            analysis_cmd += " --pheno {}".format(model["response"])
            for covar in model["covariates"]:
                analysis_cmd += " --cov {}".format(covar)

            if model["model"] == "lm":
                analysis_cmd += " --test q.linear"
            elif model["model"] == "lmm":
                analysis_cmd += " --test q.emmax --kin {}".format(geno.getKinshipPath())
            elif model["model"] == "burden":
                group = model.get("group", "nonsyn")
                analysis_cmd += " --test skat " +  \
                    "--groups {}".format(geno.getGroupsPath(group))
            else:
                raise ValueError('Unrecognize model: %s' % (model["model"],))

        return analysis_cmd

    def create_postprocessing_command(self, job_desc):
        cmd =  "if [ $EXIT_STATUS == 0 ]; then\n" 
        cmd += "  {} ./output.epacts.gz ./manhattan.json\n".format(self.config.get("MANHATTAN_BINARY", ""))
        cmd += "  {} ./output.epacts.gz ./qq.json\n".format(self.config.get("QQPLOT_BINARY", ""))
        if self.config.get("TOPHITS_BINARY"):
            cmd +=  "  {} ./output.epacts.top5000 ./tophits.json".format(self.config.get("TOPHITS_BINARY"))
            if self.config.get("NEAREST_GENE_BED"):
                cmd += " --gene " + self.config.get("NEAREST_GENE_BED") 
            cmd += "\n"
        cmd += "fi\n"
        return cmd

    def create_launch_script(self, job_desc):
        cmd = self.create_sbatch_header(job_desc) + "\n"
        cmd += "{} 2> ./err.log 1> ./out.log\n".format(self.create_analysis_command(job_desc))
        cmd += "EXIT_STATUS=$?\n"
        cmd += self.create_postprocessing_command(job_desc) + "\n" 
        cmd += "echo $EXIT_STATUS > ./exit_status.txt\n"
        cmd += "exit $EXIT_STATUS\n"
        return cmd

    def submit_job(self, job_desc):
        sbatch = self.config.get("QUEUE_JOB_BINARY", "sbatch")
        batch_script_path = self.relative_path("batch_script.sh")
        batch_output_path = self.relative_path("batch_script_output.sh")
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
        return [
            {"code":"lm", "name":" Linear Wald Test", "description": "A simple linear model"},
            {"code":"lmm", "name": "Linear Mixed Model", 
                "description": "Adjust for potential relatedness using kinship matrix"},
            {"code":"skat", "name":"SKAT-O Test", 
                "description": "Adaptive Burden Test"}
        ];
