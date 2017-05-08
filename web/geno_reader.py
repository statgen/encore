import subprocess
from collections import deque

def get_variant_id(data):
    if data[2] != ".":
        return data[2]
    else:
        return "{}:{}_{}/{}".format(data[0], data[1], data[3], data[4])

class GenoReader:
    def __init__(self, geno, config):
        self.geno = geno
        self.config = config

    def get_variant(self, chrom, pos, variant_id=None):
        vcf_path = self.geno.get_vcf_path(chrom)
        cmd = [self.config.get("TABIX_BINARY", "tabix"),
            "-h",
            vcf_path,
            "{}:{}-{}".format(chrom , pos, pos+1)]
        try:
            lines = subprocess.check_output(cmd).split("\n")
        except subprocess.CalledProcessError as e:
            raise Exception("Could not extract genotype") 
        except OSError:
            raise Exception("Could not find tabix")
        lines = deque([x for x in lines if not x.startswith("##") and len(x)!=0])
        if len(lines)<2:
            raise Exception("No variants not found")
        headers = lines.popleft().split("\t")
        headers[0] = headers[0].strip("#")
        data = lines.popleft().split("\t")
        current_variant = get_variant_id(data)
        if variant_id is not None:
            while current_variant != variant_id:
                print current_variant
                if len(lines)<1:
                    raise Exception("Variant not found ({})".format(variant_id))
                data = lines.popleft().split("\t")
                current_variant = get_variant_id(data)
        elif len(lines)>2:
            raise Exception("Multiple variants found, no ID given")
        variant_data = dict(zip(headers[0:9], data[0:9]))
        variant_data["GENOS"] = dict(zip(headers[9:], data[9:]))
        return variant_data

