import json
import os
import subprocess
import pwd
from .model_factory import ModelFactory

class SlurmJob:

    def __init__(self, job_id, job_directory, config = None):
        self.job_id = job_id
        self.job_directory = job_directory
        if config:
            self.config = config 
        else:
            self.config = dict()

    def get_batch_headers(self, model_plan):
        mem_per_cpu = model_plan.get("mem_per_cpu", self.config.get("JOB_MEM_PER_CPU", 6500))
        cores_per_job = model_plan.get("cores_per_job", self.config.get("JOB_CORES_PER_TASK", 56))
        
        sbatch_headers = ["#!/bin/bash"]

        if "SLURM_ACCOUNT" in self.config:
            sbatch_headers.append(
                "#SBATCH --account={}".format(self.config.get("SLURM_ACCOUNT")))

        sbatch_headers.append(
            "#SBATCH --partition={}".format(self.config.get("QUEUE_PARTITION", "encore")),
            "#SBATCH --job-name=gasp_{}".format(self.job_id),
            "#SBATCH --mem-per-cpu={}".format(mem_per_cpu),
            "#SBATCH --chdir={}".format(self.job_directory),
            "#SBATCH --cpus-per-task={}".format(cores_per_job),
            "#SBATCH --time={}".format(self.config.get("JOB_TIME", "14-0")),
            "#SBATCH --nodes=1",
            "export OPENBLAS_NUM_THREADS=1")

        return batch_headers

    def write_batch_script(self, batch_script_path, model_plan):
        with open(batch_script_path, "w") as f:
            f.write("\n".join(self.get_batch_headers(model_plan)))
            f.write("\n\n")
            f.write("\n".join(model_plan["commands"]))
            f.write("\n")

    def submit_job(self, model_spec):
        model = ModelFactory.get_for_model_spec(model_spec, self.job_directory, self.config)
        model_plan = model.prepare_job(model_spec)

        sbatch = self.config.get("QUEUE_JOB_BINARY", "sbatch")
        batch_script_path = self.relative_path("batch_script.sh")
        batch_output_path = self.relative_path("batch_script_output.txt")

        self.write_batch_script(batch_script_path, model_plan)
        with open(batch_output_path, "w") as f:
            try:
                subprocess.check_call([sbatch, batch_script_path], stdout=f)
            except subprocess.CalledProcessError as e:
                # log to server log
                print("SBATCH ERROR")
                print(e)
                raise Exception("Could not queue job") 
            except OSError:
                raise Exception("Could not find sbatch")
        return True

    def get_model(self, model_spec = None):
        if not model_spec:
            model_spec = self.load_model_spec()
        model = ModelFactory.get_for_model_spec(model_spec, self.job_directory, self.config)
        return model

    def resubmit_job(self):
        sbatch = self.config.get("QUEUE_JOB_BINARY", "sbatch")
        batch_script_path = self.relative_path("batch_script.sh")
        if not os.path.isfile(batch_script_path):
            raise Exception("Existing script file not found") 
        batch_output_path = self.relative_path("batch_script_output.txt")
        with open(batch_output_path, "w") as f:
            try:
                subprocess.check_call([sbatch, batch_script_path], stdout=f)
            except subprocess.CalledProcessError as e:
                # log to server log
                print("SBATCH ERROR")
                print(e)
                raise Exception("Could not queue job") 
            except OSError:
                raise Exception("Could not find sbatch")
        return True

    def cancel_job(self):
        scancel = self.config.get("CANCEL_JOB_BINARY", "scancel")
        batch_output_path = self.relative_path("batch_script_output.txt")
        try:
            with open(batch_output_path, 'r') as f:
                slurm_job_id = f.readline()
            slurm_job_id = [s for s in slurm_job_id.split() if s.isdigit()][0]
        except:
            raise Exception("Could not find job queue id ({})".format(self.job_id))
        try:
            subprocess.check_call([scancel, slurm_job_id])
        except subprocess.CalledProcessError:
            raise Exception("Could not scancel job") 
        except OSError:
            raise Exception("Could not find scancel")
        return True

    def get_progress(self):
        model = self.get_model()
        return model.get_progress()

    def load_model_spec(self):
        job_spec_path = self.relative_path("job.json")
        if not os.path.isfile(job_spec_path):
            raise Exception("Job spec file not found for job")
        with open(job_spec_path) as f:
            model_spec = json.load(f)
        return model_spec
        
    def relative_path(self, *args):
        return os.path.expanduser(os.path.join(self.job_directory, *args))

def get_queue():
    cols = [["job_id", "%i"], ["job_name", "%j"], ["state", "%t"],
        ["time", "%M"], ["reason", "%R"]]
    col_names = [x[0] for x in cols]
    col_formats = [x[1] for x in cols]
    cmd = ["/usr/cluster/bin/squeue", 
        "-u", pwd.getpwuid(os.getuid())[0], 
        "-p", "encore",
        "-o", "|".join(col_formats),
        "--noheader"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    squeue_out, squeue_err = p.communicate()
    queue = {"running": [], "queued": []}
    for line in squeue_out.decode().split("\n"):
        values = line.split("|")
        if len(values) != len(col_names):
            continue
        row = dict(zip(col_names, values))
        row["job_name"] = row["job_name"][5:]
        if row["state"]=="R":
            queue["running"].append(row)
        elif row["state"] == "PD":
            queue["queued"].append(row)
    return queue
