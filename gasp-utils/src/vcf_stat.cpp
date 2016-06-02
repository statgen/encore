
#include "vcf.h"
#include <iostream>
#include <cstdint>

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
        std::uint64_t record_cnt = 0;

        bcf1_t* rec = bcf_init1();
        while (vcf_read(hts_fp, hdr, rec) >= 0)
        {
          ++record_cnt;
        }

        std::cout << "{";
        std::cout << "\"sample_count\":" << bcf_hdr_nsamples(hdr) << ",";
        std::cout << "\"record_count\":" << record_cnt;
        std::cout << "}";

        bcf_destroy1(rec);

        bcf_hdr_destroy(hdr);
      }

      vcf_close(hts_fp);
    }
  }




  return ret;
}