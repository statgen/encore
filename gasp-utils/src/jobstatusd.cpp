#include <iostream>
#include <chrono>
#include <sys/stat.h>
#include <string>
#include <thread>
#include <vector>
#include <mysql.h>
#include <dirent.h>
#include <fstream>

bool done = false;



void close_signal_handler(int signal_code)
{
  done = true;
}

class close_mysql_conn
{
public:
  void operator()(MYSQL* c)
  {
    mysql_close(c);
  }
};

std::string escape_string(MYSQL* conn, const std::string& input)
{
  std::string ret(2 * input.size() + 1, '\0');
  std::size_t sz = mysql_real_escape_string(conn, &ret[0], input.data(), input.size());
  ret.resize(sz);
  return ret;
}

std::unique_ptr<MYSQL, close_mysql_conn> get_mysql_conn()
{
  MYSQL* ret = mysql_init(NULL);
  if (ret)
    mysql_real_connect(ret, "localhost", "gasp_user", getenv("GASP_MYSQL_PASS"), "gasp", 0, NULL, 0);
  return std::unique_ptr<MYSQL, close_mysql_conn>(ret);
}


enum class job_status
{
  invalid = 0,
  created,
  queued,
  started,
  completed
};

job_status str_to_job_status(std::string input)
{
  if (input == "created")   return job_status::created;
  if (input == "queued")    return job_status::queued;
  if (input == "started")   return job_status::started;
  if (input == "completed") return job_status::completed;
  return job_status::invalid;
}

class job
{
public:
  job(std::string uuid_string, job_status status)
    : uuid_(uuid_string), status_(status) {}
  const std::string& id() const { return uuid_; }
  job_status status() const { return status_; }
private:
  std::string uuid_;
  job_status status_;

};

bool query_pending_jobs(std::vector<job>& jobs)
{
  bool ret = false;

  auto conn = get_mysql_conn();

  if (!conn)
  {
  }
  else
  {
    std::string sql =
      "SELECT bin_to_uuid(jobs.id) AS id, statuses.name AS status FROM jobs "
      "LEFT JOIN statuses ON statuses.id = jobs.status_id "
      "WHERE statuses.name = ('created' OR statuses.name = 'queued' OR statuses.name = 'started')";

    if (mysql_query(conn.get(), sql.c_str()) != 0)
    {
    }
    else
    {
      MYSQL_RES* res = mysql_store_result(conn.get());

      jobs.reserve(mysql_num_rows(res));

      while (MYSQL_ROW row = mysql_fetch_row(res))
        jobs.emplace_back(row[0], str_to_job_status(row[1]));

      ret = true;
    }
  }

  return ret;
}

void check_for_job_status_update(const std::string& base_path, const job& j)
{
  if (j.status() == job_status::created)
  {
    std::string batch_script_path = base_path + "/" + j.id() + "/batch_script";
    std::ofstream ofs(batch_script_path);
    if (!ofs.good())
    {
    }
    else
    {
      ofs << "#!/bin/bash\n"
        << "#SBATCH --job-name=gasp_" << j.id()
        << "#SBATCH --output=output.epacts\n"
        << "#SBATCH --error=error.txt\n"
        //<< "#SBATCH --time=10:00\n"
        //<< "#SBATCH --mem-per-cpu=100\n"
        << "\n"
        << "hostname\n";

        ofs.flush();

      if (!ofs.good())
      {
      }
      else
      {
        // TODO: run sbatch batch_script_path


      }
    }
  }
  else if (j.status() == job_status::queued)
  {
  }
  else if (j.status() == job_status::started)
  {
  }
}

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

    while (done)
    {
      std::vector<job> jobs;
      if (!query_pending_jobs(jobs))
      {
        for (auto it = jobs.begin(); it != jobs.end(); ++it)
          check_for_job_status_update(base_path, *it);
      }

      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }

  return ret;
}