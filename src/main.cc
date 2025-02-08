// SPDX-License-Identifier: MIT

#include <iostream>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <curl/curl.h>

// Protobuf
#include <google/protobuf/util/json_util.h>
#include "ping/v1/messages.pb.h"
#include "ping/v1/services.pb.h"
#include "runner/v1/messages.pb.h"
#include "runner/v1/services.pb.h"
#include "connectrpc/messages.pb.h"

using namespace google::protobuf::util;

// State to track runner auth and task version
struct RunnerState
{
   uint64_t tasks_version;
   std::string endpoint;
   std::string uuid;
   std::string token;
};

// Util func to perform basic protobuf requests to a connectrpc endpoint
template<class T, class R> bool QuickRequest(CURL *curl, RunnerState& state, const char* url, T& request, R& response, connectrpc::ErrorDetail& error)
{
   // Convert to JSON
   std::string json_request;
   google::protobuf::util::JsonPrintOptions options;
   options.add_whitespace = true;
   if (!google::protobuf::util::MessageToJsonString(request, &json_request, options).ok())
   {
      return false;
   }
   
   printf("REQUEST: %s %s\n", url, json_request.c_str());
   
   // Prepare response buffer
   std::string response_data;
   
   // Set CURL options
   std::string real_url = state.endpoint;
   real_url += url;
   curl_easy_setopt(curl, CURLOPT_URL, real_url.c_str());
   curl_easy_setopt(curl, CURLOPT_POST, 1);
   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_request.c_str());
   curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_request.size());
   
   // Set headers for Connect RPC JSON
   struct curl_slist *headers = NULL;
   headers = curl_slist_append(headers, "Content-Type: application/json");
   headers = curl_slist_append(headers, "Accept: application/json");
   
   std::string uuid;
   std::string token;
   if (state.uuid.size() > 0)
   {
      uuid = "x-runner-uuid: " + state.uuid;
      headers = curl_slist_append(headers, uuid.c_str());
      token = "x-runner-token: " + state.token;
      headers = curl_slist_append(headers, token.c_str());
   }
   
   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
   
   auto writeCallback = +[](void *contents, size_t size, size_t nmemb, std::string *userp) -> size_t {
      size_t total_size = size * nmemb;
      userp->append((char*)contents, total_size);
      return total_size;
   };
   
   // Set response callback
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
   
   // Perform request
   CURLcode res = curl_easy_perform(curl);
   if (res != CURLE_OK)
   {
      printf("CURL error: %s\n", curl_easy_strerror(res));
      curl_slist_free_all(headers);
      return false;
   }
   else
   {
      JsonParseOptions parse_options;
      int http_code = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
      curl_slist_free_all(headers);
      
      if (http_code != 200)
      {
         printf("CURL error code %i\nRESPONSE:\n%s\n", http_code, response_data.c_str());
         
         if (!JsonStringToMessage(response_data, &error, parse_options).ok())
         {
            printf("CURL invalid JSON!\n");
         }
         
         return false;
      }
      else
      {
         printf("CURL RESPONSE:\n%s\n", response_data.c_str());
         
         // Assuming JSON encoding
         
         if (JsonStringToMessage(response_data, &response, parse_options).ok())
         {
            printf("Response IS JSON\n");
            return true;
         }
         else
         {
            printf("Response IS INVALID JSON\n");
            return false;
         }
         
         return true;
      }
   }
   
   return true;
}

// Util func to print out task details
void PrintTaskDetails(const runner::v1::Task& task)
{
   printf("Task ID: %lli\n", task.id());
   
   if (task.has_workflow_payload()) {
      printf("Workflow Payload: %s\n", task.workflow_payload().c_str());
   } else {
      printf("No Workflow Payload available.\n");
   }
   
   if (task.has_context()) {
      printf("Context: %s\n", task.context().DebugString().c_str());
   } else {
      printf("No Context available.\n");
   }
   
   printf("Secrets: \n");
   for (const auto& secret : task.secrets()) {
      printf("  %s\n", secret.first.c_str());
   }
   
   printf("Needs: \n");
   for (const auto& need : task.needs()) {
      printf("  %s: %s\n", need.first.c_str(), need.second.DebugString().c_str());
   }
   
   printf("Variables: \n");
   for (const auto& var : task.vars()) {
      printf("  %s: %s\n", var.first.c_str(), var.second.c_str());
   }
}

// Thread safe util class to handle updating a task.
// Note that changes are effectively batched until HealthCheck dispatches API calls.
struct TaskTracker
{
   struct LogBatch
   {
      runner::v1::UpdateLogRequest request;
      
      LogBatch(uint64_t taskID)
      {
         request.set_task_id(taskID);
      }
      
      inline int64_t index()
      {
         return request.index();
      }
      
      inline int64_t endIndex()
      {
         return request.index() + request.rows_size();
      }
   };
   
   runner::v1::Task mTask;
   runner::v1::UpdateTaskRequest mNextTaskRequest;
   runner::v1::StepState* mNextStepState;
   
   std::vector<LogBatch*> mBatchQueue;
   LogBatch* mHeadLog;
   
   uint64_t mLastLogAck;
   uint64_t mLastTaskAck;
   
   std::mutex mMutex;
   std::atomic<bool> mFinished;
   std::atomic<bool> mCancelled;
   
   TaskTracker(const runner::v1::Task& task)
   {
      mTask = task;
      mNextTaskRequest.mutable_state()->set_id(task.id());
      mLastLogAck = 0;
      mLastTaskAck = 0;
      mHeadLog = new LogBatch(task.id());
      mNextStepState = NULL;
      
      mFinished = false;
      mCancelled = false;
   }
   
   ~TaskTracker()
   {
      delete mHeadLog;
   }
   
   google::protobuf::Timestamp getNowTS()
   {
      google::protobuf::Timestamp ts;
      auto now = std::chrono::system_clock::now();
      auto duration = now.time_since_epoch();
      
      ts.set_seconds(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
      ts.set_nanos(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000);
      return ts;
   }
   
   void beginStep(uint64_t stepIDX)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      mNextStepState = mNextTaskRequest.mutable_state()->add_steps();
      mNextStepState->set_id(stepIDX);
      mNextStepState->set_result(runner::v1::RESULT_UNSPECIFIED);
      *(mNextStepState->mutable_started_at()) = getNowTS();
      mNextStepState->set_log_index(mHeadLog->endIndex());
   }
   
   void endStep(runner::v1::Result result)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      if (mNextStepState == NULL)
         return;
      mNextStepState->set_log_length(mHeadLog->endIndex() - mNextStepState->log_index());
      mNextStepState->set_result(result);
      *mNextStepState->mutable_stopped_at() = getNowTS();
      mNextStepState = NULL;
   }
   
   void log(const char* content, bool eof=false)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      runner::v1::LogRow* row = mHeadLog->request.add_rows();
      *row->mutable_time() = getNowTS();
      row->set_content(content);
      mHeadLog->request.set_no_more(eof);
      
      printf("LOG[%llu] = %s\n", mHeadLog->endIndex()-1, content);
   }
   
   LogBatch* getBatchToSend(uint64_t ack)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      uint64_t logsToErase = 0;
      
      for (auto itr = mBatchQueue.begin(),
           itrEnd = mBatchQueue.end();
           itr != itrEnd; itr++)
      {
         if ((*itr)->endIndex() < ack)
            logsToErase++;
      }
      
      for (uint64_t i=0; i<logsToErase; i++)
      {
         auto itr = mBatchQueue.begin();
         delete *itr;
         mBatchQueue.erase(itr);
      }
      
      if (mBatchQueue.empty() &&
          mHeadLog->request.rows_size() > 0)
      {
         printf("LOG[FLUSH]\n");
         mBatchQueue.push_back(mHeadLog);
         uint64_t endIndex = mHeadLog->endIndex();
         mHeadLog = new LogBatch(mTask.id());
         mHeadLog->request.set_index(endIndex);
      }
      
      return mBatchQueue.empty() ? NULL : mBatchQueue[0];
   }
   
   void clearACKS()
   {
      std::lock_guard<std::mutex> lock(mMutex);
      clearACKStates(mLastTaskAck);
      clearACKLogs(mLastLogAck);
   }
   
   void clearACKStates(uint64_t ack)
   {
      int endStep = 0;
      runner::v1::TaskState* taskState = mNextTaskRequest.mutable_state();
      
      for (int i=0; i<taskState->steps_size(); i++)
      {
         auto step = taskState->steps(i);
         if (!(step.log_index() + step.log_length() < ack))
            break;
         endStep++;
      }
      
      if (endStep > 0)
      {
         taskState->mutable_steps()->DeleteSubrange(0, endStep);
      }
   }
   
   void clearACKLogs(uint64_t ack)
   {
      auto eraseStart = mBatchQueue.begin();
      auto eraseEnd = mBatchQueue.end();
      
      for (auto itr = mBatchQueue.begin(),
           itrEnd = mBatchQueue.end();
           itr != itrEnd; itr++)
      {
         LogBatch* batch = *itr;
         if (batch->endIndex() <= ack)
         {
            delete batch;
         }
         else
         {
            eraseEnd = itr;
            break;
         }
      }
      
      if (eraseStart != eraseEnd)
      {
         mBatchQueue.erase(eraseStart, eraseEnd);
      }
   }
   
   void setResult(runner::v1::Result result)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      mNextTaskRequest.mutable_state()->set_result(result);
      *(mNextTaskRequest.mutable_state()->mutable_stopped_at()) = getNowTS();
   }
   
   void setCriticalError()
   {
      std::lock_guard<std::mutex> lock(mMutex);
      mFinished = true;
   }
   
   inline void setFinished()
   {
      mFinished = true;
   }
   
   inline bool isFinished()
   {
      return mFinished;
   }
   
   void setCancelled()
   {
      mCancelled = true;
   }
   
   inline bool isCancelled()
   {
      return mCancelled;
   }
   
   bool isACKMatching()
   {
      std::lock_guard<std::mutex> lock(mMutex);
      return mLastLogAck == mLastTaskAck;
   }
   
   void updateLogACK(uint64_t value)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      mLastLogAck = value;
   }
   
   void updateTaskState(runner::v1::Result serverResult)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      mLastTaskAck = std::min<uint64_t>(mLastTaskAck, mLastLogAck);
      if (serverResult == (runner::v1::RESULT_CANCELLED || runner::v1::RESULT_SKIPPED))
      {
         mCancelled = true;
      }
   }
   
   // Waits for ACK to match or until error condition reached
   void waitForLogSync()
   {
      uint64_t logACK = 1;
      uint64_t taskACK = 2;
      
      while (!mFinished && !mCancelled && !isACKMatching())
      {
         sleep(1);
      }
   }
};

// Sets up runner
bool SetupRunner(CURL *curl, RunnerState& state, int argc, char** argv)
{
   connectrpc::ErrorDetail error;
   // Register
   runner::v1::RegisterRequest reqRegister;
   runner::v1::RegisterResponse rspRegister;
   // Declare
   runner::v1::DeclareRequest reqDeclare;
   runner::v1::DeclareResponse rspDeclare;
   // Ping
   ping::v1::PingRequest reqPing;
   ping::v1::PingResponse rspPing;
   
   std::string name = "My Runner";
   std::string token = "NyeMYw6eVu41OKuMYfMvx3AOo8PK0XtpSD3yMmh0";
   std::string version = "1.0";
   std::vector<std::string> labels;
   state.endpoint = "http://localhost:3000";
   
   // Parse args
   for (int i = 1; i < argc; ++i) {
      if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
         name = argv[++i];
      } else if (strcmp(argv[i], "--token") == 0 && i + 1 < argc) {
         token = argv[++i];
      } else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
         version = argv[++i];
      } else if (strcmp(argv[i], "--label") == 0 && i + 1 < argc) {
         labels.push_back(argv[++i]);
      } else if (strcmp(argv[i], "--uuid") == 0 && i + 1 < argc) {
         state.uuid = argv[++i];
      } else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
         state.endpoint = argv[++i];
      } else {
         printf("Unknown argument: %s\n", argv[i]);
         return false;
      }
   }
   
   if (labels.size() == 0)
   {
      labels.push_back("funkier");
   }
   
   state.endpoint += "/api/actions/";
   reqRegister.set_name(name);
   reqRegister.set_token(token);
   reqRegister.set_version(version);
   for (const auto& label : labels)
   {
      reqRegister.add_labels(label);
   }
   
   // NOTE: we ping to see if the server is present, after which
   // FetchTask and UpdateTask basically acts like a "ping".
   
   reqPing.set_data("HELLO");
   
   if (QuickRequest(curl, state, "ping.v1.PingService/Ping",
                    reqPing, rspPing, error))
   {
      printf("Runner pinged (%s)!\n", rspPing.data().c_str());
   }
   else
   {
      printf("Runner didn't ping!\n");
      return false;
   }
   
   if (state.uuid == "")
   {
      if (QuickRequest(curl, state, "runner.v1.RunnerService/Register",
                       reqRegister, rspRegister, error))
      {
         printf("Runner registered!");
      }
      else
      {
         printf("Runner not registered!\n");
         return false;
      }
      
      state.uuid = rspRegister.runner().uuid();
      state.token = rspRegister.runner().token();
      printf("Registered UUID=%s, Token=%s\n", state.uuid.c_str(), state.token.c_str());
   }
   
   // Update
   reqDeclare.set_version("1.0");
   for (const auto& label : labels)
   {
      reqDeclare.add_labels(label);
   }
   
   if (!QuickRequest(curl, state, "runner.v1.RunnerService/Declare", reqDeclare, rspDeclare, error))
   {
      return false;
   }
   
   return true;
}

// Dispatches any queued up task updates and sends pings to server.
bool HealthCheck(CURL *curl, RunnerState& state, TaskTracker* tracker)
{
   connectrpc::ErrorDetail error;
   // FetchTask
   runner::v1::FetchTaskRequest reqTask;
   runner::v1::FetchTaskResponse rspTask;
   // UpdateTask
   runner::v1::UpdateTaskResponse rspTaskUpdate;
   // UpdateLog
   runner::v1::UpdateLogResponse rspLogUpdate;
   
   if (tracker)
   {
      // Send logs
      TaskTracker::LogBatch* newBatch = tracker->getBatchToSend(tracker->mLastLogAck);
      if (newBatch)
      {
         if (QuickRequest(curl, state, "runner.v1.RunnerService/UpdateLog",
                          newBatch->request, rspLogUpdate, error))
         {
            tracker->updateLogACK(rspLogUpdate.ack_index());
         }
      }
      
      // Send task states
      if (QuickRequest(curl, state, "runner.v1.RunnerService/UpdateTask",
                       tracker->mNextTaskRequest, rspTaskUpdate, error))
      {
         printf("===>Got update task response, seeing what we can do....\n");
         
         if (rspTaskUpdate.has_state())
         {
            tracker->updateTaskState(rspTaskUpdate.state().result());
            //PrintTaskState(rspTaskUpdate.state());
         }
         else
         {
            printf("===>Couldn't update task state!\n");
         }
      }
      
      // Clear old data
      tracker->clearACKS();
   }
   
   return true;
}

// Polls for a task on the server
TaskTracker* PollForTask(CURL* curl, RunnerState& state)
{
   TaskTracker* tracker = NULL;
   connectrpc::ErrorDetail error;
   runner::v1::FetchTaskRequest reqTask;
   runner::v1::FetchTaskResponse rspTask;
   
   reqTask.set_tasks_version(state.tasks_version);
   
   // Request more tasks
   if (QuickRequest(curl, state, "runner.v1.RunnerService/FetchTask", reqTask, rspTask, error))
   {
      // NOTE: seems to be an issue where if a workflow makes n concurrent jobs, the server will send back
      // last_version+n as the new tasks version which means tasks can be skipped if you only have one
      // worker present. As a work-around, we simply just increment the version number until we match the
      // server (that way, caching behavior can still be maintained for up to date clients).
      if (state.tasks_version < rspTask.tasks_version())
         state.tasks_version++;
      printf("Runner task version=%llu, server=%llu!\n", state.tasks_version, rspTask.tasks_version());
      
      if (rspTask.has_task())
      {
         printf("Runner has a task!\n");
         PrintTaskDetails(rspTask.task());
         tracker = new TaskTracker(rspTask.task());
      }
   }
   
   return tracker;
}

// Example thread to send dummy updates to the server
void PerformTask(TaskTracker* currentTask)
{
   currentTask->log("Will PerformTask...");
   //
   currentTask->beginStep(0);
   currentTask->log("Doing something");
   sleep(1);
   currentTask->log("Doing something else");
   currentTask->endStep(runner::v1::RESULT_SUCCESS);
   //
   currentTask->beginStep(1);
   currentTask->log("Doing something in another step");
   sleep(1);
   currentTask->log("Doing something else in another step");
   currentTask->endStep(runner::v1::RESULT_SUCCESS);
   //
   currentTask->setResult(runner::v1::RESULT_SUCCESS);
   currentTask->log("End of job reached", true);
   currentTask->waitForLogSync();
   //
   currentTask->setFinished();
}

// Entrypoint
int main(int argc, char** argv)
{
   // Initialize libcurl
   CURL *curl = curl_easy_init();
   if (!curl) {
      printf("Failed to initialize CURL\n");
      return 1;
   }
   
   RunnerState state;
   state.tasks_version = 0;
   
   if (!SetupRunner(curl, state, argc, argv))
   {
      exit(1);
   }
   
   TaskTracker* currentTask = NULL;
   std::thread* taskThread = NULL;
   uint64_t numBadPings = 0;
   const uint64_t MaxBadPings = 5;
   
   while (true)
   {
      if (!HealthCheck(curl, state, currentTask))
      {
         numBadPings++;
         if (numBadPings > MaxBadPings)
         {
            if (currentTask)
            {
               currentTask->setCancelled();
               taskThread->join();
               delete taskThread;
               taskThread = NULL;
               delete currentTask;
               currentTask = NULL;
            }
         }
         continue;
      }
      
      numBadPings = 0;
      
      // Find new task
      if (currentTask == NULL)
      {
         currentTask = PollForTask(curl, state);
         if (currentTask != NULL)
         {
            // Spin up a thread to perform this task
            taskThread = new std::thread(PerformTask, currentTask);
         }
      }
      else if (currentTask->isFinished())
      {
         taskThread->join();
         delete taskThread;
         taskThread = NULL;
         delete currentTask;
         currentTask = NULL;
      }
      
      sleep(1);
   }
   
   // Cleanup
   curl_easy_cleanup(curl);
   
   return 0;
}
