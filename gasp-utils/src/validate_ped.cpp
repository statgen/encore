#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <chrono>
#include <sys/stat.h>

int validate_ped_file(const std::string& file_path, std::ostream& err_stream)
{
  int ret = -1;

  std::regex exp("^(BB|AOS)(.*)$", std::regex_constants::optimize);

  std::ifstream ped_file(file_path);

  if (!ped_file.good())
  {
    err_stream << "Could not open ped file." << std::endl;
  }
  else
  {
    std::string ped_header_line;

    if (!std::getline(ped_file, ped_header_line, '\n'))
    {
      err_stream << "Failed to read header from ped file." << std::endl;
    }
    else
    {
      ped_header_line.erase(0, ped_header_line.find_first_not_of(" #")); // Trim prefix
      ped_header_line.erase(ped_header_line.find_last_not_of(" \r\n")+1); // Trim suffix

      std::vector<std::string> column_names;
      column_names.reserve(10); // Approximate number of columns.

      std::istringstream is(ped_header_line);
      while (is.good())
      {
        std::string tmp_column_name;
        is >> tmp_column_name;
        column_names.push_back(std::move(tmp_column_name));
      }

      long fam_id_offset = std::find(column_names.begin(), column_names.end(), "FAM_ID")-column_names.begin();
      long ind_id_offset = std::find(column_names.begin(), column_names.end(), "IND_ID")-column_names.begin();
      long dad_id_offset = std::find(column_names.begin(), column_names.end(), "DAD_ID")-column_names.begin();
      long mom_id_offset = std::find(column_names.begin(), column_names.end(), "MOM_ID")-column_names.begin();
      long sex_offset = std::find(column_names.begin(), column_names.end(), "SEX")-column_names.begin();
      long age_offset = std::find(column_names.begin(), column_names.end(), "AGE")-column_names.begin();

      if (fam_id_offset>=column_names.size()
          || ind_id_offset>=column_names.size()
          || dad_id_offset>=column_names.size()
          || mom_id_offset>=column_names.size()
          || sex_offset>=column_names.size()
          || age_offset>=column_names.size())
      {
        err_stream << "Missing required columns." << std::endl;
      }
      else
      {
        std::string data_line;
        std::size_t line_number = 1;
        bool invalid_row_found = false;
        while (!invalid_row_found && std::getline(ped_file, data_line, '\n'))
        {
          ++line_number;

          data_line.erase(data_line.find_last_not_of(" \r\n")+1);

          std::vector<std::string> fields_in_current_row;
          fields_in_current_row.reserve(column_names.size());

          std::istringstream is(data_line);
          while (is.good())
          {
            std::string tmp_field;
            is >> tmp_field;
            fields_in_current_row.push_back(std::move(tmp_field));
          }

          if (fields_in_current_row.size()!=column_names.size())
          {
            invalid_row_found = true;
            err_stream << "Number of fields in row does not match header (Line:"+std::to_string(line_number)+")."
            << std::endl;
          }
          else if (fields_in_current_row[sex_offset]!="1" && fields_in_current_row[sex_offset]!="2")
          {
            invalid_row_found = true;
            err_stream << "SEX must to be '1' or '2' (Line:"+std::to_string(line_number)+")." << std::endl;
          }
          else if (false) //!std::regex_match(fields_in_current_row[ind_id_offset], exp))
          {
            invalid_row_found = true;
            err_stream << "Invalid ID format (Line:"+std::to_string(line_number)+")." << std::endl;
          }

        }

        ret = (invalid_row_found ? -1 : 0);
      }
    }
  }

  return ret;
}

int main(int argc, char* argv[])
{
  int ret = -1;
  auto start_time = std::chrono::high_resolution_clock::now();

  if (argc!=2)
  {
    std::cerr << "Invalid number of args." << std::endl;
  }
  else
  {
    struct stat st{};
    if (stat(argv[1], &st))
    {
    }
    else
    {
      ret = validate_ped_file(argv[1], std::cerr);
    }
  }

  std::cout
  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_time).count()
  << std::endl;
  return ret;
}