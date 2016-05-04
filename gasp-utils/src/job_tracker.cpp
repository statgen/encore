#include "job_tracker.hpp"

#include <sys/stat.h>
#include <thread>
#include <vector>
#include <mysql.h>
#include <chrono>
#include <fstream>
#include <stdlib.h>
#include <ctime>
#include <iomanip>

void log(std::ostream& os, const std::string& message)
{
  auto now = std::chrono::system_clock::now();
  time_t in_time = std::chrono::system_clock::to_time_t(now);
  os << std::put_time(std::localtime(&in_time), "%Y-%m-%d %X") << ": " << message << std::endl;
}

class close_mysql_conn
{
public:
  void operator()(MYSQL* c)
  {
    mysql_close(c);
  }
};

std::unique_ptr<MYSQL, close_mysql_conn> get_mysql_conn(const std::string& password)
{
  MYSQL* ret = nullptr;
  MYSQL* my = mysql_init(NULL);
  if (my)
  {
    ret = mysql_real_connect(my, "localhost", "gasp_user", password.c_str(), "gasp", 0, NULL, 0);

    if (!ret)
      mysql_close(my);
  }
  return std::unique_ptr<MYSQL, close_mysql_conn>(ret);
}

std::string escape_string(MYSQL* conn, const std::string& input)
{
  std::string ret(2 * input.size() + 1, '\0');
  std::size_t sz = mysql_real_escape_string(conn, &ret[0], input.data(), input.size());
  ret.resize(sz);
  return ret;
}

job_status str_to_job_status(std::string input)
{
  if (input == "created")           return job_status::created;
  if (input == "queued")            return job_status::queued;
  if (input == "started")           return job_status::started;
  if (input == "succeeded")         return job_status::succeeded;
  if (input == "failed")            return job_status::failed;
  if (input == "cancel_requested")  return job_status::cancel_requested;
  if (input == "cancelled")         return job_status::cancelled;
  if (input == "quarantined")       return job_status::quarantined;
  return job_status::invalid;
}

bool job_tracker::query_pending_jobs(MYSQL* conn, std::vector<job>& jobs)
{
  bool ret = false;

  std::string sql =
    "SELECT bin_to_uuid(jobs.id) AS id, statuses.name AS status FROM jobs "
    "LEFT JOIN statuses ON statuses.id = jobs.status_id "
    "WHERE (statuses.name='created' OR statuses.name='queued' OR statuses.name='started' OR statuses.name='cancel_requested')";

  if (mysql_query(conn, sql.c_str()) != 0)
  {
  }
  else
  {
    MYSQL_RES* res = mysql_store_result(conn);

    jobs.reserve(mysql_num_rows(res));

    while (MYSQL_ROW row = mysql_fetch_row(res))
      jobs.emplace_back(row[0], str_to_job_status(row[1]));

    ret = true;
  }

  return ret;
}

void check_for_job_status_update(MYSQL* conn, const std::string& base_path, const job& j)
{
  std::string job_directory = base_path + "/" + j.id();
  std::string batch_script_path = job_directory + "/batch_script.sh";
  std::string stdout_path = job_directory + "/output.epacts";
  std::string stderr_path = job_directory + "/error.txt";
  std::string exit_status_path = job_directory + "/exit_status.txt";

  std::ofstream log_ofs(job_directory + "/log.txt", std::ios::app);
  if (!log_ofs.good())
  {
    // Log file cannot be opened. Something is horribly wrong. TODO: Contact admin.
  }
  else
  {
    if (j.status() == job_status::created)
    {
      std::ofstream ofs;
      struct stat st{};
      bool file_already_exists = !stat(batch_script_path.c_str(), &st);
      if (!file_already_exists) // Doesn't already exist.
        ofs.open(batch_script_path);
      if (file_already_exists || !ofs.good())
      {
        std::string sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name='quarantined' LIMIT 1) WHERE id = uuid_to_bin('" + escape_string(conn, j.id()) + "')";
        if (mysql_query(conn, sql.c_str()) != 0)
        {
          log(log_ofs, mysql_error(conn));
        }
        else
        {
          log(log_ofs, "Updated status to 'quarantined' (batch script already exists for job).");
        }
      }
      else
      {
        ofs
        << "#!/bin/bash\n"
        << "#SBATCH --job-name=gasp_" << j.id() << "\n"
        //<< "#SBATCH --output=" << stdout_path << "\n"
        //<< "#SBATCH --error=" << stderr_path << "\n"
        //<< "#SBATCH --time=10:00\n"
        //<< "#SBATCH --mem-per-cpu=100\n"
        << "\n"
        << "hostname 2> " << stderr_path << " 1> " << stdout_path << "\n"
        << "echo $? > " << exit_status_path << "\n";

        ofs.close();

        // TODO: run sbatch batch_script_path
        std::string bash_command = "/bin/bash " + batch_script_path;
        std::system(bash_command.c_str());

        std::string sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name='queued' LIMIT 1) WHERE id = uuid_to_bin('" + escape_string(conn, j.id()) + "')";
        if (mysql_query(conn, sql.c_str()) != 0)
        {
          log(log_ofs, mysql_error(conn));
        }
        else
        {
          log(log_ofs, "Updated status to 'queued'.");
        }
      }
    }
    else if (j.status() == job_status::queued)
    {
      struct stat st{};
      if (stat(stdout_path.c_str(), &st) == 0)
      {
        std::string sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name='started' LIMIT 1) WHERE id = uuid_to_bin('" + escape_string(conn, j.id()) + "')";
        if (mysql_query(conn, sql.c_str()) != 0)
        {
          log(log_ofs, mysql_error(conn));
        }
        else
        {
          log(log_ofs, "Updated status to 'started'.");
        }
      }
    }
    else if (j.status() == job_status::started)
    {
      std::ifstream ifs(exit_status_path.c_str());
      if (ifs.good())
      {
        int exit_status = 0;
        ifs >> exit_status;
        std::string new_job_status = exit_status ? "failed" : "succeeded";
        std::string sql = "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE name='" + new_job_status + "' LIMIT 1) WHERE id = uuid_to_bin('" + escape_string(conn, j.id()) + "')";
        if (mysql_query(conn, sql.c_str()) != 0)
        {
          log(log_ofs, mysql_error(conn));
        }
        else
        {
          log(log_ofs, "Updated status to '" + new_job_status + "'.");
        }
      }
    }
    else if (j.status() == job_status::cancel_requested)
    {
      //TODO: scancel
    }
  }
}

job_tracker::job_tracker(const std::string& base_path_for_job_folders, const std::string& mysql_pass)
  : base_path_(base_path_for_job_folders),
    mysql_pass_(mysql_pass),
    stopped_(false)
{
}

void job_tracker::operator()()
{
  while (!stopped_)
  {
    auto conn = get_mysql_conn(mysql_pass_);

    if (!conn)
    {
    }
    else
    {

      std::vector<job> jobs;
      if (query_pending_jobs(conn.get(), jobs))
      {
        for (auto it = jobs.begin(); it!=jobs.end(); ++it)
          check_for_job_status_update(conn.get(), base_path_, *it);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

void job_tracker::stop()
{
  this->stopped_.store(true);
}
