
#include "vcf.h"
#include <iostream>
#include <cstdint>
#include <vcf.h>

int main(int argc, char* argv[])
{
  int ret = -1;

  if (argc < 2)
  {
  }
  else
  {
    htsFile* hts_fp = vcf_open(argv[1], "r");
    if (!hts_fp)
    {
      std::cerr << "Could not open file" << std::endl;
    }
    else
    {
      bcf_hdr_t* hdr = vcf_hdr_read(hts_fp);

      if (!hdr)
      {
        std::cerr << "Failed to read header" << std::endl;
      }
      else
      {
        std::int32_t sample_count = bcf_hdr_nsamples(hdr);
        std::uint64_t record_count = 0;
        std::uint64_t genotype_count = 0;

        bcf1_t* rec = bcf_init1();
        while (vcf_read(hts_fp, hdr, rec) >= 0)
        {
          ++record_count;
//          bcf_info_t* ns_info = bcf_get_info(hdr, rec, "NS");
//          if (ns_info)
//            genotype_count += ns_info->v1.i;
        }

        std::cout << "{";
        std::cout << "\"genotype_count\":" << (genotype_count ? genotype_count : sample_count * record_count) << ",";
        std::cout << "\"sample_count\":" << sample_count << ",";
        std::cout << "\"record_count\":" << record_count;
        std::cout << "}";

        bcf_destroy1(rec);

        bcf_hdr_destroy(hdr);
      }

      vcf_close(hts_fp);
    }
  }




  return ret;
}