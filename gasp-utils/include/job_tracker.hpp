#ifndef GASP_UTILS_JOB_TRACKER_HPP
#define GASP_UTILS_JOB_TRACKER_HPP

#include <atomic>
#include <string>
#include <mysql.h>
#include <vector>

enum class job_status
{
  invalid = 0,
  created,
  queued,
  started,
  cancel_requested,
  cancelled,
  failed,
  succeeded,
  quarantined
};

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

class job_tracker
{
public:
  job_tracker(const std::string& base_path_for_job_folders, const std::string& mysql_db, const std::string& mysql_user, const std::string& mysql_pass);

  void operator()();
  void stop();
private:
  bool query_pending_jobs(MYSQL* conn, std::vector<job>& jobs);

  const std::string base_path_;
  const std::string mysql_db_;
  const std::string mysql_user_;
  const std::string mysql_pass_;
  std::atomic_bool stopped_;
};

#endif //GASP_UTILS_JOB_TRACKER_HPP
