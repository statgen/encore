
#include "vcf.h"
#include "synced_bcf_reader.h"
#include <iostream>
#include <cstdint>
#include <vector>

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
  invalid_file_type,
  header_read_failed
};

stat_errc stat_vcf_file(const std::string& file_path, vcf_stats& output)
{
  stat_errc ret = stat_errc::no_error;

  htsFile* hts_fp = vcf_open(file_path.c_str(), "r");
  if (!hts_fp)
  {
    ret = stat_errc::file_open_failed;
  }
  else
  {
    bcf_hdr_t* hdr = vcf_hdr_read(hts_fp);

    if (!hdr)
    {
      ret = stat_errc::header_read_failed;
    }
    else
    {
      output.sample_count = static_cast<std::uint64_t>(bcf_hdr_nsamples(hdr)); // Assuming this will never be negative.
      output.record_count = 0;
      output.genotype_count = 0;

//        bcf1_t* rec = bcf_init1();
//        while (bcf_read(hts_fp, hdr, rec) >= 0)
//        {
//          ++(output.record_count);
//          bcf_info_t* ns_info = bcf_get_info(hdr, rec, "NS");
//          if (ns_info)
//            output.genotype_count += ns_info->v1.i;
//        }
//        bcf_destroy1(rec);

      bcf_srs_t *sr = bcf_sr_init();
      if (!bcf_sr_add_reader(sr, file_path.c_str()))
      {
        ret = stat_errc::file_open_failed;
      }
      else
      {
        while ( bcf_sr_next_line(sr) )
        {
          bcf1_t* rec = bcf_sr_get_line(sr,0);
          ++(output.record_count);
          bcf_info_t* ns_info = bcf_get_info(hdr, rec, "NS");
          if (ns_info)
            output.genotype_count += ns_info->v1.i;
        }
      }
      bcf_sr_destroy(sr);

      bcf_hdr_destroy(hdr);
    }

    vcf_close(hts_fp);
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
          stats.sample_count += it->sample_count;
          stats.record_count += it->record_count;
        }

        std::cout << "{";
        std::cout << "\"genotype_count\":" << (stats.genotype_count ? stats.genotype_count : stats.sample_count * stats.record_count) << ",";
        std::cout << "\"sample_count\":" << stats.sample_count << ",";
        std::cout << "\"record_count\":" << stats.record_count;
        std::cout << "}";

        ret = 0;
      }
    }
  }




  return ret;
}