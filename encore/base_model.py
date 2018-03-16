from genotype import Genotype
from phenotype import Phenotype 
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
