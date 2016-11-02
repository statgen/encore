from genotype import Genotype
from phenotype import Phenotype 
from ped_writer import PedWriter
import os
import subprocess

class SlurmEpactsJob:

    def __init__(self, job_id, job_directory, config = None):
        self.job_id = job_id
        self.job_directory = job_directory
        if config:
            self.config = config 
        else:
            self.config = dict()
        self.cores_per_job = 56

    def create_sbatch_header(self, job_desc):
       return "#!/bin/bash\n" + \
           "#SBATCH --partition=encore\n" + \
           "#SBATCH --job-name=gasp_{}\n".format(self.job_id)  + \
           "#SBATCH --cpus-per-task={}\n".format(self.cores_per_job)  + \
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

        def one_cmd(model):
            pheno_path = self.relative_path("pheno.ped")
            pheno_cols = [model["response"]] + model.get("covariates",[])
            ped_writer = PedWriter(pheno.get_pheno_reader(), \
                model["response"], model.get("covariates",[])) 
            with open(pheno_path,"w") as pedfile:
                ped_writer.write_to_file(pedfile)
            ecmd = ""
            opts = ""
            if model.get("response_invnorm", False):
                opts += " --inv-norm" 
            model_type = model.get("type", None)
            if model_type == "lm":
                ecmd = "single"
                opts += " --test q.linear" + \
                    " --unit 500000" + \
                    " --min-maf 0.001" 
            elif model_type == "lmm":
                ecmd = "single"
                opts += " --test q.emmax --kin {}".format(geno.get_kinship_path()) + \
                    " --unit 500000" + \
                    " --min-maf 0.001" 
            elif model_type == "skato":
                ecmd = "group"
                group = model.get("group", "nonsyn")
                opts += " --test skat --skat-o" +  \
                    " --groupf {}".format(geno.get_groups_path(group)) + \
                    " --max-maf 0.05" + \
                    " --unit 500"
            elif model_type == "mmskato":
                ecmd = "group"
                group = model.get("group", "nonsyn")
                opts += " --test mmskat --skat-o" +  \
                    " --groupf {}".format(geno.get_groups_path(group)) + \
                    " --kin {}".format(geno.get_kinship_path()) + \
                    " --max-maf 0.05" + \
                    " --unit 500"
            else:
                raise ValueError('Unrecognized model type: %s' % (model_type,))

            cmd = ""
            cmd += "{} {}".format(self.config.get("ANALYSIS_BINARY", "epacts"), ecmd) + \
                " --vcf {}".format(geno.get_vcf_path(1)) + \
                " --ped {}".format(pheno_path) +  \
                " --field GT" + \
                " --sepchr" + \
                " --out ./output --run {}".format(self.cores_per_job)

            for resp in ped_writer.get_response_headers():
                cmd += " --pheno {}".format(resp)
            for covar in ped_writer.get_covar_headers():
                cmd += " --cov {}".format(covar)
            cmd += opts

            return cmd

        analysis_cmd = one_cmd(job_desc)
        return analysis_cmd

    def create_postprocessing_command(self, job_desc):
        cmd = "\n"
        cmd += "if [ -e output.epacts -a ! -e output.epacts.gz ]; then\n" + \
            "  awk 'NR<2{print;next}{print| \"sort -g -k1,1 -k2g,3\"}' output.epacts | " + \
            "/usr/cluster/bin/bgzip -c > output.epacts.gz\n" + \
            "  /usr/cluster/bin/tabix -p bed output.epacts.gz\n" + \
            "fi\n"
        cmd +=  "if [ $EXIT_STATUS == 0 ]; then\n" 
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
        return [
            {"code":"lm", "name":" Linear Wald Test", "description": "A simple linear model"},
            {"code":"lmm", "name": "Linear Mixed Model", 
                "description": "Adjust for potential relatedness using kinship matrix"} ,
            {"code":"skato", "name": "SKAT-O Test", 
                "description": "Adaptive burden test"},
            {"code":"mmskato", "name": "Mixed Model SKAT-O Test", 
                "description": "Adaptive burden test that adjusts for potential relatedness using kinship matrix"}
        ];
