from .genotype import Genotype
from .phenotype import Phenotype 
from .ped_writer import PedWriter
import os

class BaseModel(object):
    def __init__(self, working_directory=None, app_config=None):
        self.working_directory = working_directory
        self.app_config = app_config

    def get_geno(self, model_spec, geno=None):
        if geno is None:
            if "genotype" in model_spec:
                geno = Genotype.get(model_spec["genotype"], self.app_config)
            else:
                raise Exception("No genotype information in job")
        return geno

    def get_pheno(self, model_spec, pheno=None):
        if pheno is None:
            if "phenotype" in model_spec:
                pheno = Phenotype.get(model_spec["phenotype"], self.app_config)
            else:
                raise Exception("No phenotype information in job")
        return pheno

    def get_ped_writer(self, model_spec, geno, pheno):
        ped_writer = PedWriter(pheno.get_pheno_reader(), \
            model_spec["response"], model_spec.get("covariates",[]),
            set(geno.get_samples()))
        if "genopheno" in model_spec and len(model_spec["genopheno"])>0:
            ped_writer.merge_covar(geno.get_pheno_reader(), \
                model_spec["genopheno"])
        return ped_writer

    def get_response_values(self, model_spec, geno, pheno):
        return self.get_ped_writer(model_spec, geno, pheno).get_response_values()

    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.working_directory, *args))

    def if_exit_success(self, cmds, ifsuccess): 
        cmds = cmds + [ 
            "EXIT_STATUS=$?", 
            "echo $EXIT_STATUS > ./exit_status.txt",
            "if [ $EXIT_STATUS == 0 ]; then"
            ] + ifsuccess + ["fi", "exit $EXIT_STATUS"]
        return cmds

    def validate_model_spec(self, model_spec):
        pass

    def get_filter_desc(self, vfilter):
        if not hasattr(self, "filters"):
            return None
        for x in self.filters:
            if x[0] == vfilter:
                return x[1]
        return None

    def get_failure_reason(self):
        return None
