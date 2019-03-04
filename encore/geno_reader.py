import subprocess
from collections import deque

def get_epacts_variant_id(data):
    return "{}:{}_{}/{}".format(data[0], data[1], data[3], data[4])

class GenoReader:
    def __init__(self, geno, config):
        self.geno = geno
        self.config = config

    def get_variant(self, chrom, pos, variant_id=None, annotate=False):
        vcf_path = self.geno.get_vcf_path(chrom)
        variant = self.get_vcf_row(vcf_path, chrom, pos, variant_id)
        if annotate:
            anno_path = self.geno.get_vcf_anno_path(chrom)
            if anno_path:
                anno = self.get_vcf_row(anno_path, chrom, pos, variant_id)
                if variant["INFO"]:
                    variant["INFO"] = variant["INFO"] + ";" + anno["INFO"]
                elif anno["INFO"]:
                    variant["INFO"] = anno["INFO"]
        return variant

    def get_vcf_row(self, vcf_path, chrom, pos, variant_id):
        cmd = [self.config.get("TABIX_BINARY", "tabix"),
            "-h",
            vcf_path,
            "{}:{}-{}".format(chrom , pos, pos+1)]
        try:
            lines = subprocess.check_output(cmd).decode().split("\n")
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
        current_variant = get_epacts_variant_id(data)
        if variant_id is not None:
            while not variant_id.startswith(current_variant):
                if len(lines)<1:
                    raise Exception("Variant not found ({})".format(variant_id))
                data = lines.popleft().split("\t")
                current_variant = get_epacts_variant_id(data)
        if len(lines)>2:
            raise Exception("Multiple variants found, no ID given")
        if len(headers)>8:
            variant_data = dict(zip(headers[0:9], data[0:9]))
            variant_data["GENOS"] = dict(zip(headers[9:], data[9:]))
        else:
            variant_data = dict(zip(headers, data))
        return variant_data

