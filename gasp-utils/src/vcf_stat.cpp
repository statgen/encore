
#include "vcf.h"
#include <iostream>
#include <cstdint>
#include <vector>
#include <glob.h>

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

      bcf1_t* rec = bcf_init1();
      while (vcf_read(hts_fp, hdr, rec) >= 0)
      {
        ++(output.record_count);
        bcf_info_t* ns_info = bcf_get_info(hdr, rec, "NS");
        if (ns_info)
          output.genotype_count += ns_info->v1.i;
      }



      bcf_destroy1(rec);

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

    glob_t glob_buf;
    if(glob(argv[1], 0, NULL, &glob_buf) != 0)
    {
      std::cerr << "Invalid file path expression." << std::endl;
    }
    else
    {
      stat_errc res = stat_errc::no_error;
      std::vector<vcf_stats> stat_array(glob_buf.gl_pathc);
      for (std::size_t i = 0; i < glob_buf.gl_pathc && res == stat_errc::no_error; ++i) // TODO: parallelize
      {
        res = stat_vcf_file(glob_buf.gl_pathv[i], stat_array[i]);
      }

      if (res == stat_errc::file_open_failed)
      {
        std::cerr << "Could not open file." << std::endl;
      }
      else if (res == stat_errc::header_read_failed)
      {
        std::cerr << "Failed to read header." << std::endl;
      }
      else
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

      globfree(&glob_buf);
    }
  }




  return ret;
}