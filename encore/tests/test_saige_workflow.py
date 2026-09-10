import os
from pathlib import Path
import shutil
import subprocess

import pytest
import yaml


REPO_ROOT = Path(__file__).resolve().parents[2]
SNAKEFILE = REPO_ROOT / "scripts" / "saige" / "snakefile-saige"
SNAKEMAKE = os.environ.get("SNAKEMAKE") or shutil.which("snakemake")


def _write_input_files(workdir, chroms, include_male_file=False):
    sav_dir = workdir / "savs"
    sav_dir.mkdir()
    for chrom in chroms:
        (sav_dir / f"{chrom}.sav").touch()

    pheno_file = workdir / "pheno.ped"
    pheno_file.write_text("IND_ID\ttrait\n1\t1\n")
    sample_file = workdir / "samples.txt"
    sample_file.write_text("1\n")

    male_file = None
    if include_male_file:
        male_file = workdir / "male_samples.txt"
        male_file.write_text("1\n")

    return sav_dir, pheno_file, sample_file, male_file


def _write_config(
    workdir,
    chroms,
    trait_type="quantitative",
    covars=None,
    categorical_covars=None,
    include_male_file=False,
):
    sav_dir, pheno_file, sample_file, male_file = _write_input_files(
        workdir, chroms, include_male_file
    )
    config = {
        "contigs": chroms,
        "savs_path": str(sav_dir),
        "samples_file": str(sample_file),
        "samplesfile_male": str(male_file) if male_file else None,
        "bgzip": "/usr/bin/bgzip",
        "tabix": "/usr/bin/tabix",
        "output_dir": ".",
        "phenoFile": str(pheno_file),
        "plinkFile": str(workdir / "plink"),
        "sampleIDColinphenoFile": "IND_ID",
        "traitType": trait_type,
        "outputPrefix": "step1",
        "nThreads": 2,
        "IsOverwriteVarianceRatioFile": "TRUE",
        "response": "trait",
        "inv_norm": trait_type == "quantitative",
        "min_maf": 0.001,
        "min_mac": 20,
        "covarColList": ",".join(covars or []),
        "qCovarColList": ",".join(categorical_covars or []),
    }
    config_path = workdir / "config.yaml"
    config_path.write_text(yaml.safe_dump(config))
    return config_path


def _add_reference_config(config_path, workdir, lengths, region_size):
    fasta = workdir / "reference.fa"
    fasta.touch()
    fai = workdir / "reference.fa.fai"
    fai.write_text(
        "".join(f"{chrom}\t{length}\t0\t0\t0\n" for chrom, length in lengths.items())
    )
    config = yaml.safe_load(config_path.read_text())
    config["reference_fasta"] = str(fasta)
    config["region_size"] = region_size
    config_path.write_text(yaml.safe_dump(config))


def _dry_run(workdir, config_path):
    if not SNAKEMAKE:
        pytest.skip("snakemake is not installed; run these tests inside the SAIGE SIF")

    result = subprocess.run(
        [
            SNAKEMAKE,
            "--snakefile",
            str(SNAKEFILE),
            "--configfile",
            str(config_path),
            "--cores",
            "2",
            "--dry-run",
            "--printshellcmds",
        ],
        cwd=workdir,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    assert result.returncode == 0, result.stdout
    return result.stdout


def test_quantitative_autosome_with_covariates(tmp_path):
    config_path = _write_config(
        tmp_path,
        ["chr1"],
        covars=["age", "sex"],
        categorical_covars=["sex"],
    )

    output = _dry_run(tmp_path, config_path)

    assert "--covarColList=age,sex" in output
    assert "--qCovarColList=sex" in output
    assert "--invNormalize=TRUE" in output
    assert "--minMAF=0.001" in output
    assert "--minMAC=20" in output
    assert "--is_Firth_beta=FALSE" in output
    assert "--sampleFile_male=" not in output


def test_autosome_without_covariates_omits_arguments(tmp_path):
    config_path = _write_config(tmp_path, ["chr2"])

    output = _dry_run(tmp_path, config_path)

    assert "--covarColList=" not in output
    assert "--qCovarColList=" not in output
    assert "step2.bin.chr2.txt" in output


@pytest.mark.parametrize(
    ("configured_filters", "expected_options"),
    [
        ({}, set()),
        ({"min_mac": 20}, {"--minMAC=20"}),
        ({"min_maf": 0.001}, {"--minMAF=0.001"}),
        ({"min_maf": 0.001, "min_mac": 20}, {"--minMAF=0.001", "--minMAC=20"}),
    ],
)
def test_step2_only_adds_filters_present_in_config(
    tmp_path, configured_filters, expected_options
):
    config_path = _write_config(tmp_path, ["chr2"])
    config = yaml.safe_load(config_path.read_text())
    config.pop("min_maf")
    config.pop("min_mac")
    config.update(configured_filters)
    config_path.write_text(yaml.safe_dump(config))

    output = _dry_run(tmp_path, config_path)

    for option in expected_options:
        assert option in output
    if "min_maf" not in configured_filters:
        assert "--minMAF=" not in output
    if "min_mac" not in configured_filters:
        assert "--minMAC=" not in output


def test_fasta_index_splits_chromosome_into_regions(tmp_path):
    config_path = _write_config(tmp_path, ["chr20"])
    _add_reference_config(config_path, tmp_path, {"chr20": 1200}, region_size=500)

    output = _dry_run(tmp_path, config_path)

    assert "step2.bin.chr20.1.500.txt" in output
    assert "step2.bin.chr20.501.1000.txt" in output
    assert "step2.bin.chr20.1001.1200.txt" in output
    assert '--rangestoIncludeFile="$range_file"' in output
    assert 'grep -qF "No markers are left in VCF"' in output


def test_binary_chr_x_uses_x_and_firth_arguments(tmp_path):
    config_path = _write_config(
        tmp_path,
        ["chrX"],
        trait_type="binary",
        include_male_file=True,
    )

    output = _dry_run(tmp_path, config_path)

    assert "--invNormalize=FALSE" in output
    assert "--is_Firth_beta=TRUE" in output
    assert "--sampleFile_male=" in output
    assert "--X_PARregion=" in output
    assert "--is_rewrite_XnonPAR_forMales=TRUE" in output


def test_chr_x_requires_male_sample_file(tmp_path):
    config_path = _write_config(tmp_path, ["chrX"], trait_type="binary")

    if not SNAKEMAKE:
        pytest.skip("snakemake is not installed; run these tests inside the SAIGE SIF")

    result = subprocess.run(
        [
            SNAKEMAKE,
            "--snakefile",
            str(SNAKEFILE),
            "--configfile",
            str(config_path),
            "--cores",
            "2",
            "--dry-run",
        ],
        cwd=tmp_path,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )

    assert result.returncode != 0
    assert "samplesfile_male" in result.stdout
