
#include "vcf.h"
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>

struct vcf_stats
{
  std::uint64_t genotype_count;
  std::uint64_t sample_count;
  std::uint64_t record_count;
  vcf_stats()
    : genotype_count(0), sample_count(0), record_count(0) {}
};

enum class stat_errc
{
  no_error = 0,
  file_open_failed,
  header_read_failed
};

stat_errc stat_vcf_file(const std::string& file_path, vcf_stats& output)
{
  stat_errc ret = stat_errc::no_error;

  htsFile* hts_fp = bcf_open(file_path.c_str(), "r");
  if (!hts_fp)
  {
    ret = stat_errc::file_open_failed;
  }
  else
  {
    bcf_hdr_t* hdr = bcf_hdr_read(hts_fp);

    if (!hdr)
    {
      ret = stat_errc::header_read_failed;
    }
    else
    {
      output.sample_count = static_cast<std::uint64_t>(bcf_hdr_nsamples(hdr)); // Assuming this will never be negative.
      output.record_count = 0;
      output.genotype_count = 0;

      bcf1_t* rec = bcf_init1();
      while (bcf_read(hts_fp, hdr, rec) >= 0)
      {
        ++(output.record_count);
        bcf_info_t* ns_info = bcf_get_info(hdr, rec, "NS");
        if (ns_info)
          output.genotype_count += ns_info->v1.i;

        /*int* gt_arr = NULL;
        int ngt_arr = 0;
        int ngt = bcf_get_genotypes(hdr, rec, &gt_arr, &ngt_arr);
        for (int j = 0; j < output.sample_count; ++j)
        {
          int* igt = (gt_arr + j * 2);
          bool missing = (bool)bcf_gt_is_missing(gt_arr[j * 2]);
          int a = bcf_gt_allele(gt_arr[j * 2]);
          int b = bcf_gt_allele(gt_arr[j * 2 + 1]);
          bool phased = (bool)bcf_gt_is_phased(gt_arr[j * 2 + 1]);
          int foo = a + b;
        }*/
      }

      if (!output.genotype_count)
        output.genotype_count = output.record_count * output.sample_count;

      bcf_destroy1(rec);

      bcf_hdr_destroy(hdr);
    }

    bcf_close(hts_fp);
  }

  return ret;
}

int main(int argc, char* argv[])
{
  int ret = -1;

  if (argc < 2)
  {
  }
  else
  {
    vcf_stats stats;

    int nthreads = 1;

    int arg_itr = 1;
    while (arg_itr < argc)
    {
      if (argv[arg_itr][0] != '-')
        break;

      if (strcmp(argv[arg_itr], "-t") == 0 && ++arg_itr < argc)
      {
        nthreads = atoi(argv[arg_itr]);
      }

      ++arg_itr;
    }

    if (arg_itr == argc)
    {
      std::cerr << "You must specify a file path." << std::endl;
    }
    else if (nthreads < 1)
    {
      std::cerr << "Invalid number of threads specified." << std::endl;
    }
    else
    {
      const int path_offset = arg_itr;
      const std::size_t path_count = (std::size_t)(argc - path_offset);

      std::vector<vcf_stats> stat_array(path_count);
      std::vector<stat_errc> stat_res_array(path_count, stat_errc::no_error);

      #pragma omp parallel for num_threads(nthreads)
      for (std::size_t i = 0; i < path_count; ++i)
      {
        stat_res_array[i] = stat_vcf_file(argv[path_offset + i], stat_array[i]);
      }

      bool all_jobs_succeeded = true;
      for (std::size_t i = 0; i < path_count; ++i)
      {
        if (stat_res_array[i] == stat_errc::file_open_failed)
        {
          std::cerr << "Could not open file (" << argv[path_offset + i] << ")." << std::endl;
          all_jobs_succeeded = false;
        }
        else if (stat_res_array[i] == stat_errc::header_read_failed)
        {
          std::cerr << "Failed to read header (" << argv[path_offset + i] << ")." << std::endl;
          all_jobs_succeeded = false;
        }
      }


      if (all_jobs_succeeded)
      {
        // merge
        for (auto it = stat_array.begin(); it != stat_array.end(); ++it)
        {
          stats.genotype_count += it->genotype_count;
          stats.sample_count = std::max(stats.sample_count, it->sample_count);
          stats.record_count += it->record_count;
        }

        std::cout << "{";
        std::cout << "\"genotype_count\":" << stats.genotype_count  << ",";
        std::cout << "\"sample_count\":" << stats.sample_count << ",";
        std::cout << "\"record_count\":" << stats.record_count;
        std::cout << "}";

        ret = 0;
      }
    }
  }

  return ret;
}
