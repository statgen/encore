#include <iostream>
#include "job_tracker.hpp"

int main(int argc, char *argv[])
{
  int ret = -1;

  if (argc != 2)
  {
    std::cerr << "Invalid number of args." << std::endl;
  }
  else
  {
    std::string base_path(argv[1]);
    base_path.erase(base_path.find_last_not_of(" /") + 1);


    const char* pass = getenv("GASP_MYSQL_DB");
    const char* user = getenv("GASP_MYSQL_USER");
    const char* db = getenv("GASP_MYSQL_PASS");
    job_tracker tracker(base_path, db ? db : "", user ? user : "", pass ? pass : "" );
    tracker();
  }

  return ret;
}