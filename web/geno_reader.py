import subprocess

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
        lines = [x for x in lines if not x.startswith("##") and len(x)!=0]
        if len(lines)==2:
            headers = lines[0].split("\t")
            headers[0] = headers[0].strip("#")
            data = lines[1].split("\t")
            variant_data = dict(zip(headers[0:9], data[0:9]))
            variant_data["GENOS"] = dict(zip(headers[9:], data[9:]))
        elif len(lines)>2:
            raise Exception("Multiple variants found")
        else:
            raise Exception("Variant not found")
        return variant_data

