/*

Copyright (c) 2025 James Urquhart

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

SPDX-License-Identifier: GPL-3.0-or-later

*/

#include <iostream>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <atomic>
#include <curl/curl.h>
#include <fkYAML/node.hpp>

// Protobuf
#include <google/protobuf/util/json_util.h>
#include "ping/v1/messages.pb.h"
#include "ping/v1/services.pb.h"
#include "runner/v1/messages.pb.h"
#include "runner/v1/services.pb.h"
#include "connectrpc/messages.pb.h"

#include "expressionEval.h"
#include "subprocess.h"

using ExprValue = ExpressionEval::ExprValue;
using ExprObject = ExpressionEval::ExprObject;
using ExprArray = ExpressionEval::ExprArray;
using ExprMap = ExpressionEval::ExprMap;
using ExprFieldObject = ExpressionEval::ExprFieldObject;
using ExprMultiKey = ExpressionEval::ExprMultiKey;
using ExprState = ExpressionEval::ExprState;
using FuncInfo = ExpressionEval::FuncInfo;

using namespace google::protobuf::util;

enum
{
   REnv_Core=0,
   REnv_Workflow=1,
   REnv_Job=2,
   REnv_Step=3,
   REnv_Force,
   REnv_COUNT
};

// State to track runner auth and task version
struct RunnerState
{
   uint64_t tasks_version;
   std::string endpoint;
   std::string uuid;
   std::string token;
   runner::v1::Runner info;
};

const char* RunnerStatusToString(runner::v1::RunnerStatus status)
{
   static const char* names[] = {
      "",
      "idle",
      "active",
      "offline"
   };
   uint8_t key = std::min<uint8_t>((uint8_t)status, (uint8_t)(sizeof(names) / sizeof(names[0])));
   return names[key];
}

const char* RunnerResultToString(runner::v1::Result res)
{
   static const char* names[] = {
      "success",
      "failure",
      "cancelled",
      "skipped"
   };
   uint8_t key = std::min<uint8_t>((uint8_t)res, (uint8_t)(sizeof(names) / sizeof(names[0])));
   return names[key];
}

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

struct JobStepDefinition;
struct SingleWorkflowDefinition;

#ifdef _WIN32
    #include <windows.h>
    #define GET_PID() GetCurrentProcessId()
#else
    #include <unistd.h>
    #define GET_PID() getpid()
#endif

std::unordered_map<std::string, std::string> ParseEnvFile(const std::string &filename);

struct ShellExecutor
{
   ExprState* mState;
   RunnerState* mRunnerState;
   std::string mJobID;
   std::function<void(const char*, std::size_t)> mLogHandler;
   
   ShellExecutor(ExprState* state) : mState(state), mRunnerState(NULL)
   {
   }
   
   static void dumpEnv(ExprObject* map, std::vector<std::string>& values, std::vector<const char*>& valuesChar)
   {
      map->extractKeys(values);
      for (size_t i=0; i<values.size(); i++)
      {
         ExprValue value = map->getMapKey(values[i]);
         if (value.isBool())
         {
            values[i] = values[i] + std::string("=") + (value.getBool() ? "true" : "false");
         }
         else if (value.isString())
         {
            values[i] = values[i] + std::string("=") + value.getString();
         }
         else
         {
            values[i] = values[i] + std::string("=") + std::to_string(value.getNumber());
         }
         valuesChar.push_back(values[i].c_str());
      }
   }
   
   virtual runner::v1::Result execute(const std::string& cwd,
                                      const std::string& shell,
                                      const std::string& cmdList,
                                      ExprMultiKey* env,
                                      ExprObject* outEnv,
                                      ExprObject* outputs) = 0;
   
   virtual ~ShellExecutor()
   {
   }
};

struct UnixShellExecutor : public ShellExecutor
{
   struct subprocess_s mSubprocess;
   
   UnixShellExecutor(ExprState* state) : ShellExecutor(state)
   {
   }
   
   runner::v1::Result execute(const std::string& cwd,
                              const std::string& shell,
                              const std::string& cmdList,
                              ExprMultiKey* env,
                              ExprObject* outEnv,
                              ExprObject* outputs) override
   {
      // Define the file name
      std::filesystem::path filePath = std::filesystem::temp_directory_path() / (std::to_string(GET_PID()) + "-run-" + mJobID + ".sh");
      std::filesystem::path outputPath = std::filesystem::temp_directory_path() / (std::to_string(GET_PID()) + "-run-" + mJobID + "-outputs.env");
      std::filesystem::path envPath = std::filesystem::temp_directory_path() / (std::to_string(GET_PID()) + "-run-" + mJobID + "-env.env");
      
      // Update step env
      env->mSlots[REnv_Step]->setMapKey("GITHUB_ENV", ExprValue().setString(*mState->mStringTable, envPath.c_str()));
      env->mSlots[REnv_Step]->setMapKey("GITHUB_OUTPUT", ExprValue().setString(*mState->mStringTable, outputPath.c_str()));
      env->mSlots[REnv_Step]->setMapKey("GITHUB_PATH", ExprValue().setString(*mState->mStringTable, "")); // TODO
      
      // Load env
      
      std::vector<std::string> envS;
      std::vector<const char*> envC;
      
      if (env != NULL)
      {
         dumpEnv(env, envS, envC);
      }
      
      envC.push_back(NULL);
      
      // Write temp script
      
      std::ofstream outFile(filePath, std::ios::binary);
      outFile << "set -e\n";
      outFile << "cd \"" + cwd + "\"\n";
      outFile << cmdList;
      outFile << "\n";
      outFile.close();
      std::filesystem::remove(outputPath);
      std::filesystem::remove(envPath);
      
      const char *command_line[] = {shell.c_str(), filePath.c_str(), NULL};
      int result = subprocess_create_ex(command_line,
                                        subprocess_option_enable_async |
                                        subprocess_option_combined_stdout_stderr,
                                        &envC[0], &mSubprocess);
      
      // Poll logs
      
      char buffer[4096];
      char* curLine = buffer;
      buffer[0] = '\0';

      while (true)
      {
          // Read into the available buffer space
          size_t bytesAvailable = sizeof(buffer) - 1 - (curLine - buffer);
          if (bytesAvailable < 0)
          {
             break;
          }
         
          size_t bytesRead = subprocess_read_stdout(&mSubprocess, (char*)curLine, (unsigned)bytesAvailable);
          
          if (bytesRead <= 0)
          {
              break; // No more data to read
          }

          curLine[bytesRead] = '\0'; // Null-terminate
          const char* eof = buffer + strlen(buffer);
          const char* nextLine = buffer;

          printf("DEBUG SUBPROCESS:");
          fwrite(buffer, bytesRead, 1, stdout);
          printf("\n");

          while (nextLine < eof)
          {
              // Find the next '\n'
              const char* lineEnd = strchr(nextLine, '\n');

              if (!lineEnd)
              {
                  break; // Incomplete line, move remaining bytes to start
              }

              // Adjust length to strip trailing '\r' if present
              size_t lineLength = lineEnd - nextLine;
              if (lineLength > 0 && nextLine[lineLength - 1] == '\r')
              {
                  lineLength--; // Exclude trailing '\r'
              }

              // Emit the log line
              mLogHandler(nextLine, lineLength);

              // Move past the newline
              nextLine = lineEnd + 1;
          }

          // Move leftover data to the start of the buffer
          size_t remaining = eof - nextLine;
          if (remaining > 0)
          {
              memmove(buffer, nextLine, remaining);
          }
          
          curLine = buffer + remaining; // Update buffer position for next read

          // If buffer is completely full and no newline was found, emit as a full line
          if (curLine == buffer + sizeof(buffer) - 1)
          {
              mLogHandler(buffer, curLine - buffer);
              curLine = buffer; // Reset buffer
          }
      }
      
      int process_return = 0;
      subprocess_join(&mSubprocess, &process_return);
      //subprocess_terminate(&mSubprocess);
      
      // Cleanup
      std::filesystem::remove(filePath);
      
      if (std::filesystem::exists(envPath))
      {
         if (outEnv)
         {
            // Load env
            auto kv = ParseEnvFile(envPath);
            for (auto& itr : kv)
            {
               outEnv->setMapKey(itr.first, ExprValue().setString(*mState->mStringTable, itr.second.c_str()));
            }
         }
         std::filesystem::remove(envPath);
      }
      if (std::filesystem::exists(outputPath))
      {
         if (outputs)
         {
            // Load output
            auto kv = ParseEnvFile(outputPath);
            for (auto& itr : kv)
            {
               outputs->setMapKey(itr.first, ExprValue().setString(*mState->mStringTable, itr.second.c_str()));
            }
            std::filesystem::remove(outputPath);
         }
      }
      
      return process_return != 0 ?  runner::v1::RESULT_FAILURE : runner::v1::RESULT_SUCCESS;
   }
};


// Thread safe util class to handle updating a task.
// Note that changes are effectively batched until HealthCheck dispatches API calls.
class TaskTracker
{
public:
   
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
 
private:
   RunnerState mRunner;
   runner::v1::Task mTask;
   runner::v1::UpdateTaskRequest mNextTaskRequest;
   runner::v1::StepState* mNextStepState;
   
   std::vector<LogBatch*> mBatchQueue;
   LogBatch* mHeadLog;
   
   uint64_t mLastLogAck;
   uint64_t mLastTaskAck;
   
   ExprState* mExprState;
   SingleWorkflowDefinition* mWorkflow;
   
   std::mutex mMutex;
   std::mutex mJobInfoMutex;
   std::atomic<bool> mFinished;
   std::atomic<bool> mCancelled;
   
public:
   
   TaskTracker(const RunnerState& runner, const runner::v1::Task& task, ExprState* state, SingleWorkflowDefinition* ctx)
   {
      mTask = task;
      mRunner = runner;
      mNextTaskRequest.mutable_state()->set_id(task.id());
      mLastLogAck = 0;
      mLastTaskAck = 0;
      mHeadLog = new LogBatch(task.id());
      mNextStepState = NULL;
      mExprState = state;
      mWorkflow = ctx;
      
      mFinished = false;
      mCancelled = false;
   }
   
   ~TaskTracker()
   {
      // Cleanup
      delete mHeadLog;
   }
   
   inline ExprState* _getExprState()
   {
      return mExprState;
   }
   
   inline runner::v1::Task& _getTask()
   {
      return mTask;
   }
   
   inline RunnerState& _getRunner()
   {
      return mRunner;
   }
   
   inline SingleWorkflowDefinition* _getWorkflow() const
   {
      return mWorkflow;
   }
   
   void beginJob()
   {
      mJobInfoMutex.lock();
   }
   
   void endJob()
   {
      mJobInfoMutex.unlock();
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
   
   void getUpdate(runner::v1::UpdateTaskRequest& outReq)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      outReq = mNextTaskRequest;
      mNextTaskRequest.mutable_outputs()->clear();
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
      
      if (ack == 0)
      {
         ack = mLastLogAck;
      }
      
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
   
private:
   
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
   
public:
   
   void addOutput(std::string& name, const std::string& value)
   {
      std::lock_guard<std::mutex> lock(mMutex);
      mLastTaskAck = std::min<uint64_t>(mLastTaskAck, mLastLogAck);
      mNextTaskRequest.mutable_outputs()->insert({name, value});
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
   void _waitForLogSync()
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
   
   if (rspDeclare.has_runner())
   {
      state.info = rspDeclare.runner();
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
      TaskTracker::LogBatch* newBatch = tracker->getBatchToSend(0);
      if (newBatch)
      {
         if (QuickRequest(curl, state, "runner.v1.RunnerService/UpdateLog",
                          newBatch->request, rspLogUpdate, error))
         {
            tracker->updateLogACK(rspLogUpdate.ack_index());
         }
      }
      
      // Send task states
      runner::v1::UpdateTaskRequest nextUpdate;
      tracker->getUpdate(nextUpdate);
      
      if (QuickRequest(curl, state, "runner.v1.RunnerService/UpdateTask",
                       nextUpdate, rspTaskUpdate, error))
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

// Converts YAML to an ExprValue, using an already constructed ExprObject if specified and the type matches
ExprValue YAMLToExpr(ExprState& state, ExprObject* baseObject, fkyaml::node& node)
{
   ExprArray* baseArray = dynamic_cast<ExprArray*>(baseObject);
   ExprMap* baseMap = dynamic_cast<ExprMap*>(baseObject);
   
   ExprValue ret;
   if (node.is_sequence())
   {
      baseArray = baseObject ? baseArray : new ExprArray(&state);
      if (baseArray == NULL)
         return;
      
      for (auto& itr : node.as_seq())
      {
         ExprValue val = YAMLToExpr(state, NULL, itr);
         baseArray->mItems.push_back(val);
      }
      ret.setObject(baseArray);
   }
   else if (node.is_mapping())
   {
      ExpressionEval::ExprMap* baseMap = baseObject ? baseMap : new ExpressionEval::ExprMap(&state);
      if (baseMap == NULL)
         return;
      
      for (auto& itr : node.as_map())
      {
         ExprValue val = YAMLToExpr(state, NULL, itr.second);
         baseMap->mItems[itr.first.as_str()] = val;
      }
      ret.setObject(baseMap);
   }
   else if (node.is_float_number() && baseObject == NULL)
   {
      ret.setNumeric(node.as_float());
   }
   else if (node.is_boolean() && baseObject == NULL)
   {
      ret.setBool(node.as_bool());
   }
   else if (node.is_string() && baseObject == NULL)
   {
      ret.setString(*state.mStringTable, node.as_str().c_str());
   }
   
   return ret;
}

struct SingleWorkflowDefinition : public ExprFieldObject
{
   ExprValue mName;
   ExprValue mOn;
   ExprValue mEnv;
   ExprValue mJobs;
   ExprValue mDefaults;
   
   SingleWorkflowDefinition(ExprState* state);
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<SingleWorkflowDefinition>(); }
};

template<> void ExprFieldObject::registerFieldsForType<SingleWorkflowDefinition>()
{
   registerField<SingleWorkflowDefinition>("name", offsetof(SingleWorkflowDefinition, mName), ExprValue::STRING);
   registerField<SingleWorkflowDefinition>("on", offsetof(SingleWorkflowDefinition, mOn), ExprValue::OBJECT);
   registerField<SingleWorkflowDefinition>("env", offsetof(SingleWorkflowDefinition, mEnv), ExprValue::OBJECT);
   registerField<SingleWorkflowDefinition>("jobs", offsetof(SingleWorkflowDefinition, mJobs), ExprValue::OBJECT, false);
   registerField<SingleWorkflowDefinition>("defaults", offsetof(SingleWorkflowDefinition, mDefaults), ExprValue::OBJECT);
}

// Context shared with jobs and steps
struct BasicContext : public ExprFieldObject
{
   typedef ExprFieldObject Parent;
   
   runner::v1::Task* mTask;
   ExprValue mName;
   ExprValue mId;
   
   ExprValue mConditional;
   
   ExprValue mInputs;
   ExprValue mOutputs;
   ExprValue mEnv;
   
   ExprValue mTimeoutMinutes;
   ExprValue mContinueOnError;
   
   ExprValue mUses;
   ExprValue mWith;
   
   BasicContext(ExprState* state) : ExprFieldObject(state)
   {
      mTask = NULL;
      mTimeoutMinutes = ExprValue().setNumeric(10);
      mContinueOnError = ExprValue().setBool(false);
      mEnv.setObject(new ExprMap(mState));
   }
   
   std::string toString() override { return ""; }
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<BasicContext>(); }
};


template<> void ExprFieldObject::registerFieldsForType<BasicContext>()
{
   registerField<BasicContext>("uses", offsetof(BasicContext, mUses), ExprValue::STRING);
   registerField<BasicContext>("with", offsetof(BasicContext, mWith), ExprValue::OBJECT);
   registerField<BasicContext>("name", offsetof(BasicContext, mName), ExprValue::STRING);
   registerField<BasicContext>("id", offsetof(BasicContext, mId), ExprValue::STRING);
   registerField<BasicContext>("if", offsetof(BasicContext, mConditional), ExprValue::STRING);
   registerField<BasicContext>("inputs", offsetof(BasicContext, mInputs), ExprValue::OBJECT);
   registerField<BasicContext>("outputs", offsetof(BasicContext, mOutputs), ExprValue::OBJECT);
   registerField<BasicContext>("env", offsetof(BasicContext, mEnv), ExprValue::OBJECT, false);
   registerField<BasicContext>("timeout-minutes", offsetof(BasicContext, mTimeoutMinutes), ExprValue::NUMBER);
   registerField<BasicContext>("continue-on-error", offsetof(BasicContext, mContinueOnError), ExprValue::BOOLEAN);
}

struct ContainerContext;
struct StrategyContext;
struct ServicesContext;
struct PermissionsContext;

struct JobDefinition : public BasicContext
{
   typedef BasicContext Parent;
   
   ExprValue mContainer;
   ExprValue mStrategy;
   ExprValue mServices;
   ExprValue mPermissions;
   ExprValue mDefaults;
   ExprValue mSteps;
   
   ExprValue mNeeds;
   ExprValue mRunsOn;
   ExprValue mConcurrencyGroup;
   
   ExprValue mSecrets;
   
   JobDefinition(ExprState* state);
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<JobDefinition>(); }
};

template<> void ExprFieldObject::registerFieldsForType<JobDefinition>()
{
   auto& parentReg = getFieldRegistry<JobDefinition::Parent>();
   getFieldRegistry<JobDefinition>().insert(parentReg.begin(), parentReg.end());
   registerField<JobDefinition>("container", offsetof(JobDefinition, mContainer), ExprValue::OBJECT);
   registerField<JobDefinition>("strategy", offsetof(JobDefinition, mStrategy), ExprValue::STRING);
   registerField<JobDefinition>("services", offsetof(JobDefinition, mServices), ExprValue::OBJECT);
   registerField<JobDefinition>("permissions", offsetof(JobDefinition, mPermissions), ExprValue::OBJECT);
   registerField<JobDefinition>("defaults", offsetof(JobDefinition, mDefaults), ExprValue::OBJECT);
   registerField<JobDefinition>("steps", offsetof(JobDefinition, mSteps), ExprValue::OBJECT, false);
   registerField<JobDefinition>("needs", offsetof(JobDefinition, mNeeds), ExprValue::OBJECT);
   registerField<JobDefinition>("runs-on", offsetof(JobDefinition, mRunsOn), ExprValue::STRING);
   registerField<JobDefinition>("concurrency-group", offsetof(JobDefinition, mConcurrencyGroup), ExprValue::STRING);
   registerField<JobDefinition>("secrets", offsetof(JobDefinition, mSecrets), ExprValue::OBJECT, false);
}

struct JobStepContext;

struct JobStepsState
{
   std::vector<JobStepDefinition*> steps;
   std::vector<JobStepContext*> stepContexts;
   ExprMap* stepsMap;
   ExprMap* jobOutput;
   ExprMultiKey* env;
};

struct SingleStepState
{
   JobStepDefinition* definition;
   JobStepContext* context;
   ExprMap* outputStep;
   ExprMap* outputEnv;
   ExprMultiKey* env;
   int index;
};

class JobStepExecutor
{
public:
   typedef std::function<JobStepExecutor*()> CreateFunc;
   
   struct StepResult
   {
      runner::v1::Result result;
      bool doContinue;
   };
   
   ExprState* mState;
   TaskTracker* mTask;
   SingleStepState mStepState;

   JobStepExecutor() : mState(NULL), mTask(NULL)
   {
   }
   virtual ~JobStepExecutor()
   {
   }
   
   virtual StepResult execute()
   {
      return {runner::v1::RESULT_FAILURE, false};
   }
   
   template<class T> static T* createExecutor() { return new T(); }
   
    static void registerExecutor(const std::string& key, CreateFunc createFunc)
    {
        getRegistry()[key] = createFunc;
    }

    static CreateFunc getExecutor(const std::string& key)
    {
        auto& registry = getRegistry();
        return registry.count(key) ? registry[key] : registry["default"];
    }

private:
   
    static std::unordered_map<std::string, CreateFunc>& getRegistry()
    {
        static std::unordered_map<std::string, CreateFunc> registry;
        return registry;
    }
};

struct JobStepDefinition : public BasicContext
{
   typedef BasicContext Parent;
   
   ExprValue mRun;
   ExprValue mShell;
   ExprValue mCwd;
   
   JobStepDefinition(ExprState* state) : BasicContext(state)
   {
   }
   
   virtual ~JobStepDefinition() = default;
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<JobStepDefinition>(); }
};

struct JobResultContext : public ExprFieldObject
{
   ExprValue mResult;
   ExprValue mOutputs;
   
   JobResultContext(ExprState* state);
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<JobResultContext>(); }
};

template<> void ExprFieldObject::registerFieldsForType<JobResultContext>()
{
   registerField<JobResultContext>("result", offsetof(JobResultContext, mResult), ExprValue::STRING);
   registerField<JobResultContext>("outputs", offsetof(JobResultContext, mOutputs), ExprValue::OBJECT);
}

struct CurrentJobContext : public ExprFieldObject
{
   ExprValue mContainer;
   ExprValue mServices;
   ExprValue mStatus;
   
   CurrentJobContext(ExprState* state);
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<CurrentJobContext>(); }
};

template<> void ExprFieldObject::registerFieldsForType<CurrentJobContext>()
{
   registerField<CurrentJobContext>("container", offsetof(CurrentJobContext, mContainer), ExprValue::OBJECT, false);
   registerField<CurrentJobContext>("services", offsetof(CurrentJobContext, mServices), ExprValue::OBJECT, false);
   registerField<CurrentJobContext>("status", offsetof(CurrentJobContext, mStatus), ExprValue::STRING);
}

CurrentJobContext::CurrentJobContext(ExprState* state) : ExprFieldObject(state)
{
   ExprMap* map = new ExprMap(state);
   map->mAddObjectFunc = ExprMap::addTypedObjectFunc<ExprMap>;
   mContainer = ExprValue().setObject(map);
   map = new ExprMap(state);
   map->mAddObjectFunc = ExprMap::addTypedObjectFunc<ExprMap>;
   mServices = ExprValue().setObject(map);
}


JobResultContext::JobResultContext(ExprState* state) : ExprFieldObject(state)
{
   ExprMap* map = new ExprMap(state);
   map->mAddObjectFunc = ExprMap::addTypedObjectFunc<ExprMap>;
   mOutputs = ExprValue().setObject(map);
}

struct JobStepContext : public ExprFieldObject
{
   ExprValue mOutputs;
   ExprValue mConclusion;
   ExprValue mOutcome;
   
   JobStepContext(ExprState* state);
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<JobStepContext>(); }
};

template<> void ExprFieldObject::registerFieldsForType<JobStepContext>()
{
   registerField<JobStepContext>("outputs", offsetof(JobStepContext, mOutputs), ExprValue::OBJECT);
   registerField<JobStepContext>("conclusion", offsetof(JobStepContext, mConclusion), ExprValue::STRING);
   registerField<JobStepContext>("outcome", offsetof(JobStepContext, mOutcome), ExprValue::STRING);
}

JobStepContext::JobStepContext(ExprState* state) : ExprFieldObject(state)
{
   ExprMap* map = new ExprMap(state);
   map->mAddObjectFunc = ExprMap::addTypedObjectFunc<ExprMap>;
   mOutputs = ExprValue().setObject(map);
}


struct RunnerInfo : public ExprFieldObject
{
   ExprValue mId;
   ExprValue mUUID;
   ExprValue mToken;
   ExprValue mName;
   ExprValue mStatus;
   ExprValue mLabels;
   // gh stuff
   ExprValue mOs;
   ExprValue mArch;
   ExprValue mTemp;
   ExprValue mToolCache;
   ExprValue mDebug;
   ExprValue mEnvironment;
   
   RunnerInfo(ExprState* state) : ExprFieldObject(state)
   {
      mLabels = ExprValue().setObject(new ExprArray(state));
   }
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<RunnerInfo>(); }
};

template<> void ExprFieldObject::registerFieldsForType<RunnerInfo>()
{
   registerField<RunnerInfo>("id", offsetof(RunnerInfo, mId), ExprValue::STRING);
   registerField<RunnerInfo>("uuid", offsetof(RunnerInfo, mUUID), ExprValue::STRING);
   registerField<RunnerInfo>("token", offsetof(RunnerInfo, mToken), ExprValue::STRING);
   registerField<RunnerInfo>("name", offsetof(RunnerInfo, mName), ExprValue::STRING);
   registerField<RunnerInfo>("status", offsetof(RunnerInfo, mStatus), ExprValue::STRING);
   registerField<RunnerInfo>("labels", offsetof(RunnerInfo, mLabels), ExprValue::OBJECT, false);
   //
   registerField<RunnerInfo>("os", offsetof(RunnerInfo, mOs), ExprValue::STRING);
   registerField<RunnerInfo>("arch", offsetof(RunnerInfo, mArch), ExprValue::STRING);
   registerField<RunnerInfo>("temp", offsetof(RunnerInfo, mTemp), ExprValue::STRING);
   registerField<RunnerInfo>("tool_cache", offsetof(RunnerInfo, mToolCache), ExprValue::STRING);
   registerField<RunnerInfo>("debug", offsetof(RunnerInfo, mDebug), ExprValue::BOOLEAN);
   registerField<RunnerInfo>("environment", offsetof(RunnerInfo, mEnvironment), ExprValue::STRING);
}

template<> void ExprFieldObject::registerFieldsForType<JobStepDefinition>()
{
   auto& parentReg = getFieldRegistry<JobStepDefinition::Parent>();
   getFieldRegistry<JobStepDefinition>().insert(parentReg.begin(), parentReg.end());
   registerField<JobStepDefinition>("run", offsetof(JobStepDefinition, mRun), ExprValue::STRING);
   registerField<JobStepDefinition>("shell", offsetof(JobStepDefinition, mShell), ExprValue::STRING);
   registerField<JobStepDefinition>("cwd", offsetof(JobStepDefinition, mCwd), ExprValue::STRING);
}

const char* GetArchName()
{
#if defined(__x86_64__) || defined(_M_X64)
    return "X64";
#elif defined(__i386__) || defined(_M_IX86)
    return "X86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
#elif defined(__arm__) || defined(_M_ARM)
    return "ARM";
#elif defined(__powerpc64__) || defined(__ppc64__)
    return "PPC64";
#elif defined(__powerpc__) || defined(__ppc__)
    return "PPC";
#elif defined(__riscv) && (__riscv_xlen == 64)
    return "RV64";
#elif defined(__riscv) && (__riscv_xlen == 32)
    return "RV32";
#else
    return "UNKNOWN";
#endif
}

const char* GetOSName()
{
#if defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(__APPLE__) && defined(__MACH__)
   return "macOS";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__NetBSD__)
    return "NetBSD";
#elif defined(__OpenBSD__)
    return "OpenBSD";
#elif defined(__unix__) || defined(__unix)
    return "Unix";
#else
    return "Unknown";
#endif
}

std::string Trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::unordered_map<std::string, std::string> ParseEnvFile(const std::string &filename)
{
    std::ifstream file(filename);
    std::unordered_map<std::string, std::string> outputVars;
    std::string line, key, value;
    bool inMultiline = false;
    std::string delimiter = "";
    
    if (!file)
    {
       printf("Error: Could not open file %s\n", filename.c_str());
       return {};
    }

    while (getline(file, line))
    {
        line = Trim(line);

        if (line.empty())
        {
           continue;
        }
       
        if (inMultiline)
        {
            if (line == delimiter)
            {
                outputVars[key] = value;
                inMultiline = false;
                key = "";
                value = "";
                delimiter = "";
            }
            else
            {
                value += (value.empty() ? "" : "\n") + line;
            }
        }
        else
        {
            size_t pos = line.find('=');
            if (pos != std::string::npos)
            {
                key = Trim(line.substr(0, pos));
               std::string val = Trim(line.substr(pos + 1));

                if (val.size() > 2 && val.substr(0, 2) == "<<")
                {
                    delimiter = val.substr(2);
                    inMultiline = true;
                    value = "";
                }
                else
                {
                    outputVars[key] = val;
                }
            }
        }
    }

    file.close();
    return outputVars;
}

void SetRunnerInfoFromProto(ExprState& state, runner::v1::Runner& info, RunnerInfo* outInfo)
{
   outInfo->mId.setString(*state.mStringTable, std::to_string(info.id()).c_str());
   outInfo->mUUID.setString(*state.mStringTable, info.uuid().c_str());
   outInfo->mToken.setString(*state.mStringTable, info.token().c_str());
   outInfo->mName.setString(*state.mStringTable, info.name().c_str());
   outInfo->mStatus.setString(*state.mStringTable, RunnerStatusToString(info.status()));
   
   ExprArray* labelList = dynamic_cast<ExprArray*>(outInfo->mLabels.getObject());
   if (labelList)
   {
      labelList->clear();
      for (auto& itr : info.labels())
      {
         labelList->addArrayValue(ExprValue().setString(*state.mStringTable, itr.c_str()));
      }
   }
   
   outInfo->mOs.setString(*state.mStringTable, GetOSName());
   outInfo->mArch.setString(*state.mStringTable, GetArchName());
   outInfo->mTemp.setString(*state.mStringTable, "/tmp");
   outInfo->mToolCache.setString(*state.mStringTable, "");
   outInfo->mDebug.setBool(false);
   outInfo->mEnvironment.setString(*state.mStringTable, "self-hosted");
}


SingleWorkflowDefinition::SingleWorkflowDefinition(ExprState* state) : ExprFieldObject(state)
{
   ExprMap* jobMap = new ExprMap(state);
   jobMap->mAddObjectFunc = ExprArray::addTypedObjectFunc<JobDefinition>;
   mJobs.setObject(jobMap);
   //mSecrets.setObject(new ExprMap(state));
}

JobDefinition::JobDefinition(ExprState* state) : BasicContext(state)
{
   ExprArray* jobArray = new ExprArray(state);
   jobArray->mAddObjectFunc = ExprArray::addTypedObjectFunc<JobStepDefinition>;
   mSteps = ExprValue().setObject(jobArray);
}


ExprValue YAMLToExpr(ExprState& state, ExprObject* baseObject, const fkyaml::node& node)
{
   ExprArray* baseArray = dynamic_cast<ExprArray*>(baseObject);
   ExpressionEval::ExprMap* baseMap = dynamic_cast<ExpressionEval::ExprMap*>(baseObject);
   
   ExprValue ret;
   if (node.is_sequence() && (baseObject == NULL || baseArray))
   {
      if (baseArray == NULL)
      {
         baseArray = new ExprArray(&state);
      }
      
      for (auto& itr : node.as_seq())
      {
         ExprValue val = YAMLToExpr(state, NULL, itr);
         baseArray->mItems.push_back(val);
      }
      
      ret.setObject(baseArray);
   }
   else if (node.is_mapping() && (baseObject == NULL || baseMap))
   {
      if (baseMap == NULL)
      {
         baseMap = new ExpressionEval::ExprMap(&state);
      }
      
      for (auto& itr : node.as_map())
      {
         ExprValue val = YAMLToExpr(state, NULL, itr.second);
         baseMap->mItems[itr.first.as_str()] = val;
      }
      
      ret.setObject(baseMap);
   }
   else if (node.is_float_number())
   {
      ret.setNumeric(node.as_float());
   }
   else if (node.is_boolean())
   {
      ret.setBool(node.as_bool());
   }
   else if (node.is_string())
   {
      ret.setString(*state.mStringTable, node.as_str().c_str());
   }
   
   return ret;
}


// input: obj: root node object
// input: yaml_map root node of serialized ExprFieldObject
void YAMLToExprField(ExprState& state,
                     ExprFieldObject* obj,
                     const fkyaml::node::mapping_type& yaml_map)
{
   auto& registry = obj->getObjectFieldRegistry();
   for (auto field : registry)
   {
      std::string key = field.second.baseName;
      auto itr = yaml_map.find(key);
      
      if (itr != yaml_map.end())
      {
         if (!field.second.canSet)
         {
            // Grab object under key
            ExprObject* inst = obj->getMapKey(key).getObject();
            if (inst == NULL)
            {
               continue;
            }
            
            // Key is a map, field is a field object -> set the fields
            ExprFieldObject* exprObj = dynamic_cast<ExprFieldObject*>(inst);
            if (exprObj && itr->second.is_mapping())
            {
               YAMLToExprField(state, exprObj, itr->second.as_map());
               continue;
            }
            
            // Key is an array, field is an array
            ExprArray* exprArray = dynamic_cast<ExprArray*>(inst);
            if (exprArray && itr->second.is_sequence())
            {
               exprArray->clear();
               for (auto arrayItr : itr->second.as_seq())
               {
                  // Additional consideration: Array might have a constructor for its items
                  ExprObject* arrayItem = exprArray->constructObject();
                  ExprValue arrayItemValue;
                  if (arrayItem)
                  {
                     ExprFieldObject* fieldArrayItem = dynamic_cast<ExprFieldObject*>(arrayItem);
                     if (fieldArrayItem)
                     {
                        YAMLToExprField(state, fieldArrayItem, arrayItr.as_map());
                     }
                     else
                     {
                        YAMLToExpr(state, arrayItem, arrayItr);
                     }
                     arrayItemValue.setObject(fieldArrayItem);
                  }
                  else
                  {
                     arrayItemValue = YAMLToExpr(state, NULL, itr->second);
                  }
                  exprArray->addArrayValue(arrayItemValue);
               }
               continue;
            }
            
            // Key is a map, field is a map
            ExprMap* exprMap = dynamic_cast<ExprMap*>(inst);
            if (exprMap && itr->second.is_mapping())
            {
               exprMap->clear();
               for (auto mapItr : itr->second.as_map())
               {
                  // Additional consideration: Array might have a constructor for its items
                  ExprObject* mapItem = exprMap->constructObject();
                  ExprValue mapItemValue;
                  if (mapItem)
                  {
                     ExprFieldObject* fieldMapItem = dynamic_cast<ExprFieldObject*>(mapItem);
                     if (fieldMapItem)
                     {
                        YAMLToExprField(state, fieldMapItem, mapItr.second.as_map());
                     }
                     else
                     {
                        YAMLToExpr(state, mapItem, mapItr);
                     }
                     mapItemValue.setObject(fieldMapItem);
                  }
                  else
                  {
                     mapItemValue = YAMLToExpr(state, NULL, mapItr.second);
                  }
                  exprMap->setMapKey(mapItr.first.as_str(), mapItemValue);
               }
            }
            
            // Out of options
         }
         else
         {
            // Directly replace the key
            obj->setMapKey(key, YAMLToExpr(state, NULL, itr->second));
        }
      }
      
   }
}

ExprMap* ProtoKVToObject(ExprState& state, const ::google::protobuf::Map<std::string, std::string>& map)
{
   ExprMap* outMap = new ExprMap(&state);
   for (auto& kv: map)
   {
      outMap->setMapKey(kv.first, ExprValue().setString(*state.mStringTable, kv.second.c_str()));
   }
   return outMap;
}

ExprMap* ProtoStructToObject(ExprState& state, const ::google::protobuf::Struct& map);

ExprValue ProtoValueToExprValue(ExprState& state, const ::google::protobuf::Value& value)
{
   ExprValue outValue;
   
   switch (value.kind_case()) {
      case google::protobuf::Value::kNullValue:
         break;
      case google::protobuf::Value::kNumberValue:
         outValue.setNumeric(value.number_value());
         break;
      case google::protobuf::Value::kStringValue:
         outValue.setString(*state.mStringTable, value.string_value().c_str());
         break;
      case google::protobuf::Value::kBoolValue:
         outValue.setBool(value.bool_value());
         break;
      case google::protobuf::Value::kStructValue:
         outValue.setObject(ProtoStructToObject(state, value.struct_value()));
         break;
      case google::protobuf::Value::kListValue:
      {
         ExprArray* arrayObject = new ExprArray(&state);
         for (int i = 0; i < value.list_value().values_size(); ++i)
         {
            arrayObject->addArrayValue(ProtoValueToExprValue(state, value.list_value().values(i)));
         }
         outValue.setObject(arrayObject);
      }
      default:
         break;
   }
   
   return outValue;
}

ExprMap* ProtoStructToObject(ExprState& state, const ::google::protobuf::Struct& map)
{
   ExprMap* outMap = new ExprMap(&state);
   
   for (const auto& field : map.fields())
   {
       const std::string& key = field.first;
       const google::protobuf::Value& value = field.second;
       outMap->setMapKey(key, ProtoValueToExprValue(state, value));
   }
   
   return outMap;
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
         std::vector<JobStepDefinition*> steps;

         if (!rspTask.task().has_workflow_payload())
         {
            return NULL;
         }
         
         ExprState* exprState = new ExprState();
         
         exprState->mStringTable = new ExpressionEval::StringTable();
         SingleWorkflowDefinition* workFlowContext = new SingleWorkflowDefinition(exprState);

         try
         {
            auto node = fkyaml::node::deserialize(rspTask.task().workflow_payload());
            if (!node.is_mapping())
            {
               throw std::runtime_error("YAML isn't a map");
            }
            
            YAMLToExprField(*exprState, workFlowContext, node.as_map());
            
            std::vector<std::string> jobList;
            workFlowContext->mJobs.getObject()->extractKeys(jobList);
            if (jobList.size() == 0)
            {
               throw std::runtime_error("No jobs defined in workflow");
            }
            else if (jobList.size() > 1)
            {
               throw std::runtime_error("More than one job defined in workflow");
            }
            
         }
         catch (const std::exception& e)
         {
            // Cleanup
            delete exprState;
            return NULL;
         }

         tracker = new TaskTracker(state, rspTask.task(), exprState, workFlowContext);
      }
   }
   
   return tracker;
}

template<class T> void getTypedObjectsFromArray(ExprArray* arr, std::vector<T*>& outList)
{
   if (arr == NULL)
   {
      return;
   }
   for (ExprValue val : arr->mItems)
   {
      if (val.isObject())
      {
         T* obj = dynamic_cast<T*>(val.getObject());
         if (obj)
         {
            outList.push_back(obj);
         }
      }
   }
}

ExprObject* EvaluateAllKeys(ExprObject* obj)
{
   if (obj == NULL)
   {
      return NULL;
   }
   std::vector<std::string> keyList;
   obj->extractKeys(keyList);
   for (std::string& key : keyList)
   {
      ExprValue val = obj->getMapKey(key);
      if (val.containsExpression())
      {
         obj->setMapKey(key, obj->mState->evaluateString(val.getString()));
      }
   }
   return obj;
}

void GetStepStateSteps(ExprState* exprState, ExprArray* stepList, JobStepsState& state)
{
   getTypedObjectsFromArray<JobStepDefinition>(stepList, state.steps);
   
   // Create live steps
   state.stepContexts.reserve(state.steps.size());
   for (JobStepDefinition* def : state.steps)
   {
      if (def->mId.getString() == NULL)
      {
         state.stepContexts.push_back(NULL);
      }
      else
      {
         JobStepContext* stepCtx = new JobStepContext(exprState);
         state.stepContexts.push_back(stepCtx);
         state.stepsMap->setMapKey(def->mId.getString(), ExprValue().setObject(stepCtx));
      }
   }
}

void SetupExprState(TaskTracker* currentTask, ExprState* exprState)
{
   // Set core env
   ExprMap* needsMap = new ExprMap(exprState);
   ExprMap* coreEnv = new ExprMap(exprState);
   ExprMultiKey* env = new ExprMultiKey(exprState);
   ExprArray* jobsList = new ExprArray(exprState);
   ExprMap* stepsList = new ExprMap(exprState);
   RunnerInfo* runnerInfo = new RunnerInfo(exprState);
   CurrentJobContext* currentJobContext = new CurrentJobContext(exprState);
   ExprMap* gitContext = NULL;
   ExprObject* runnerCtx = NULL;
   
   // Set env...
   env->mSlots[REnv_Core] = coreEnv;
   env->mSlots[REnv_Workflow] = EvaluateAllKeys(currentTask->_getWorkflow()->mEnv.getObject());
   
   
   // NOTE: "needs" and "jobs" are basically the same
   for (auto itr : currentTask->_getTask().needs())
   {
      ExprMap* outMap = ProtoKVToObject(*exprState, itr.second.outputs());
      JobResultContext* jobCtx = new JobResultContext(exprState);
      jobCtx->mOutputs.setObject(outMap);
      jobCtx->mResult.setString(*exprState->mStringTable, RunnerResultToString(itr.second.result()));
      needsMap->setMapKey(itr.first, ExprValue().setObject(jobCtx));
      jobsList->addArrayValue(ExprValue().setObject(jobCtx));
   }
   
   gitContext = ProtoStructToObject(*exprState, currentTask->_getTask().context());
   
   SetRunnerInfoFromProto(*exprState, currentTask->_getRunner().info, runnerInfo);
   
   // Setup context for job
   exprState->setContext("github", gitContext);
   exprState->setContext("env", env);
   exprState->setContext("vars", ProtoKVToObject(*exprState, currentTask->_getTask().vars()));
   exprState->setContext("job", currentJobContext);
   exprState->setContext("jobs", jobsList); // NOTE: only used in reusable workflows
   exprState->setContext("steps", stepsList);
   exprState->setContext("runner", runnerInfo);
   exprState->setContext("secrets", ProtoKVToObject(*exprState, currentTask->_getTask().secrets()));
   exprState->setContext("needs", needsMap);
   exprState->setContext("inputs", gitContext->getMapKey("inputs").getObject()); // NOTE: only used in reusable workflows
   // NOTE: no matrix info is sent down so these are placeholders for now
   exprState->setContext("strategy", new ExprMap(exprState));
   exprState->setContext("matrix", new ExprMap(exprState));
   
   // Setup core env
   coreEnv->setMapKey("GITHUB_ACTION",gitContext->getMapKey("action"));
   coreEnv->setMapKey("GITHUB_ACTIONS", ExprValue().setBool(false));
   coreEnv->setMapKey("GITHUB_ACTOR", ExprValue().setString(*exprState->mStringTable, "unknown"));
   coreEnv->setMapKey("GITHUB_API_URL",gitContext->getMapKey("api_url"));
   coreEnv->setMapKey("GITHUB_BASE_REF",gitContext->getMapKey("base_ref"));
   coreEnv->setMapKey("GITHUB_EVENT_NAME",gitContext->getMapKey("event_name"));
   coreEnv->setMapKey("GITHUB_EVENT_PATH", ExprValue().setString(*exprState->mStringTable, "")); // TODO?
   coreEnv->setMapKey("GITHUB_GRAPHQL_URL",gitContext->getMapKey("graphql_url"));
   coreEnv->setMapKey("GITHUB_HEAD_REF",gitContext->getMapKey("head_ref"));
   coreEnv->setMapKey("GITHUB_JOB",gitContext->getMapKey("job"));
   coreEnv->setMapKey("GITHUB_REF",gitContext->getMapKey("ref"));
   coreEnv->setMapKey("GITHUB_REF_NAME",gitContext->getMapKey("ref_name"));
   coreEnv->setMapKey("GITHUB_REF_PROTECTED",gitContext->getMapKey("ref_protected"));
   coreEnv->setMapKey("GITHUB_REF_TYPE",gitContext->getMapKey("ref_type"));
   coreEnv->setMapKey("GITHUB_REPOSITORY",gitContext->getMapKey("repository"));
   coreEnv->setMapKey("GITHUB_REPOSITORY_ID",gitContext->getMapKey("event").getObject()->getMapKey("repository").getObject()->getMapKey("id"));
   coreEnv->setMapKey("GITHUB_REPOSITORY_OWNER",gitContext->getMapKey("repository_owner"));
   coreEnv->setMapKey("GITHUB_REPOSITORY_OWNER_ID",gitContext->getMapKey("repository_owner_id"));
   coreEnv->setMapKey("GITHUB_RETENTION_DAYS", ExprValue().setString(*exprState->mStringTable, "0")); // TODO
   coreEnv->setMapKey("GITHUB_RUN_ATTEMPT",gitContext->getMapKey("run_attempt"));
   coreEnv->setMapKey("GITHUB_RUN_ID",gitContext->getMapKey("run_id"));
   coreEnv->setMapKey("GITHUB_RUN_NUMBER",gitContext->getMapKey("run_number"));
   coreEnv->setMapKey("GITHUB_SERVER_URL",gitContext->getMapKey("server_url"));
   coreEnv->setMapKey("GITHUB_SHA",gitContext->getMapKey("sha"));
   coreEnv->setMapKey("GITHUB_STEP_SUMMARY",ExprValue().setString(*exprState->mStringTable, "")); // TODO
   coreEnv->setMapKey("GITHUB_TOKEN",gitContext->getMapKey("token"));
   coreEnv->setMapKey("GITHUB_TRIGGERING_ACTOR",gitContext->getMapKey("triggering_actor"));
   coreEnv->setMapKey("GITHUB_WORKFLOW",gitContext->getMapKey("workflow"));
   coreEnv->setMapKey("GITHUB_WORKSPACE",gitContext->getMapKey("workspace"));
   
   // Setup runner env
   runnerCtx = exprState->getContext("runner");
   coreEnv->setMapKey("RUNNER_ARCH", runnerCtx->getMapKey("arch"));
   coreEnv->setMapKey("RUNNER_DEBUG", runnerCtx->getMapKey("debug").coerceStringValue(*exprState->mStringTable));
   coreEnv->setMapKey("RUNNER_NAME", ExprValue().setString(*exprState->mStringTable, "unknown"));
   coreEnv->setMapKey("RUNNER_OS", runnerCtx->getMapKey("os"));
   coreEnv->setMapKey("RUNNER_TEMP", runnerCtx->getMapKey("temp"));
   coreEnv->setMapKey("RUNNER_TOOL_CACHE", runnerCtx->getMapKey("tool_cache"));
   
   coreEnv->setMapKey("CI", ExprValue().setString(*exprState->mStringTable, "true"));
   
   //coreEnv->setMapKey("GITHUB_ENV", ExprValue().setString(*exprState->mStringTable, gitContext->getMapKey("base_ref")));
}

class JobExecutor
{
public:
   ExprState* mState;
   JobDefinition* mJob;
   std::string mJobName;
   TaskTracker* mTask;
   int32_t mCurrentStep;
   int32_t mNumSteps;
   
   JobExecutor() : mState(NULL), mJob(NULL), mTask(NULL), mCurrentStep(0), mNumSteps(0)
   {
   }
   
   virtual ~JobExecutor()
   {
   }
   
   void finishTask(runner::v1::Result result, const char* endMessage)
   {
      // Skip
      for (int i=mCurrentStep; i<mNumSteps; i++)
      {
         if (i < 0)
         {
            continue;
         }
         mTask->beginStep(i);
         mTask->endStep(result);
      }
      mTask->setResult(result);
      mTask->log(endMessage, true);
      mTask->_waitForLogSync();
   }
   
   virtual void handleJob() = 0;
   
   typedef std::function<JobExecutor*()> CreateFunc;
   
   template<class T> static T* createExecutor() { return new T(); }
   
    static void registerExecutor(const std::string& key, CreateFunc createFunc)
    {
        getRegistry()[key] = createFunc;
    }

    static CreateFunc getExecutor(const std::string& key)
    {
        auto& registry = getRegistry();
        return registry.count(key) ? registry[key] : registry["default"];
    }

private:
   
    static std::unordered_map<std::string, CreateFunc>& getRegistry()
    {
        static std::unordered_map<std::string, CreateFunc> registry;
        return registry;
    }
};

class StandardStepExecutor : public JobStepExecutor
{
public:
   
   StepResult execute() override
   {
      char buffer[512];
      bool continueOnError = mStepState.definition->mContinueOnError.getBool();
      
      EvaluateAllKeys(dynamic_cast<ExprMap*>(mStepState.definition->mEnv.getObject())); // resolve keys for STEP
      
      snprintf(buffer, sizeof(buffer), "Doing something in step %i...", mStepState.index);
      mTask->log(buffer);
      
      ExprMap* stepMap = mStepState.context ? mStepState.context->mOutputs.asObject<ExprMap>() : NULL;
      runner::v1::Result result = internalExecute();
      
      if (mStepState.context)
      {
         mStepState.context->mOutcome.setString(*mState->mStringTable, RunnerResultToString(result));
         mStepState.context->mConclusion.setString(*mState->mStringTable, RunnerResultToString(result));
      }
      
      return { result, continueOnError };
   }
   
   
   runner::v1::Result internalExecute()
   {
      std::string cmdToRun = mState->substituteExpressions(mStepState.definition->mRun.getStringSafe());
      char buffer[4096];
      snprintf(buffer, sizeof(buffer), "TODO: run %s", cmdToRun.c_str());
      mTask->log(buffer);
      
      TaskTracker* task = mTask;
      UnixShellExecutor shellExec(mState);
      shellExec.mJobID = std::to_string(mTask->_getTask().id());
      shellExec.mLogHandler = [task](const char* data, size_t len) {
         std::string sdata(data, len);
         task->log(sdata.c_str());
      };
      
      std::string cwd = mStepState.definition->mCwd.getString() ? mStepState.definition->mCwd.getString() : std::filesystem::current_path().c_str();
      std::string shell = mStepState.definition->mShell.getString() ? mStepState.definition->mShell.getString() : "/bin/bash";
      
      shellExec.execute(cwd, shell, cmdToRun, mStepState.env, mStepState.outputEnv, mStepState.outputStep);
      
      return runner::v1::RESULT_SUCCESS;
   }
   
};

class StandardJobExecutor : public JobExecutor
{
public:
   virtual void handleJob()
   {
      SetupExprState(mTask, mState);
      
      // Gen step contexts
      JobStepsState stepState;
      stepState.stepsMap = (ExprMap*)mState->getContext("steps");
      stepState.env = (ExprMultiKey*)mState->getContext("env");
      stepState.jobOutput = mJob->mOutputs.asObject<ExprMap>();
      GetStepStateSteps(mState, mJob->mSteps.asObject<ExprArray>(), stepState);
      
      mCurrentStep = 0;
      mNumSteps = (int32_t)stepState.steps.size();
      
      // Make sure we have job env set...
      stepState.env->mSlots[REnv_Job] = EvaluateAllKeys(mJob->mEnv.getObject());
      
      printf("Running job %s\n", mJobName.c_str());
      // Check if job has "if"
      if (mJob->mConditional.getString())
      {
         std::string conditional = mJob->mConditional.getString();
         ExprValue conditionalValue = mState->substituteSingleExpression(conditional);
         if (conditionalValue.getBool() == false)
         {
            finishTask(runner::v1::RESULT_SKIPPED, "Job conditional test failed");
            return;
         }
         else
         {
            mTask->log("Job conditional test passed");
         }
      }
      
      performTaskSteps(stepState);
   }
   
   void performTaskSteps(JobStepsState& stepState)
   {
      char buffer[4096];
      
      ExprMap* gitContext = (ExprMap*)mState->getContext("github");
      
      // SETUP DONE
      snprintf(buffer, sizeof(buffer), "Performing %i tasks for job %s...", (int)stepState.steps.size(), mJobName.c_str());
      mTask->log(buffer);
     
      //getTypedObjectsFromArray
      uint32_t stepCount = 0;
      runner::v1::Result jobResult = runner::v1::RESULT_SUCCESS;
      
      for (size_t i=0; i<stepState.steps.size(); i++)
      {
         JobStepDefinition* step = stepState.steps[i];
         JobStepContext* stepCtx = stepState.stepContexts[i];
         
         // Update context for step
         stepState.env->mSlots[REnv_Step] = step->mEnv.getObject();
         mState->setContext("inputs", gitContext->getMapKey("event").getObject()->getMapKey("inputs").getObject());
         
         mTask->beginStep(stepCount);
         
         // Check if step has "if"
         if (step->mConditional.getString())
         {
            std::string conditional = step->mConditional.getString();
            ExprValue conditionalValue = mState->substituteSingleExpression(conditional);
            if (conditionalValue.getBool() == false)
            {
               // Skip job
               if (stepCtx)
               {
                  stepCtx->mOutcome.setString(*mState->mStringTable, RunnerResultToString(runner::v1::RESULT_SKIPPED));
               }
               mTask->log("Step conditional test failed");
               mTask->endStep(runner::v1::RESULT_SKIPPED);
               stepCount++;
               continue;
            }
            else
            {
               mTask->log("Step conditional test passed");
            }
         }
         
         // Find executor
         
         const char* stepUses = step->mUses.getString();
         if (stepUses == NULL)
         {
            stepUses = "default";
         }
         
         JobStepExecutor::StepResult stepResult = {runner::v1::RESULT_FAILURE, false};
         JobStepExecutor::CreateFunc executorFunc = JobStepExecutor::getExecutor(stepUses);
         if (executorFunc == NULL)
         {
            std::string err = std::string("Error: couldn't find executor: ") + stepUses;
            mTask->log(err.c_str());
         }
         else
         {
            JobStepExecutor* executor = executorFunc();
            
            SingleStepState singleState;
            singleState.definition = step;
            singleState.context = stepCtx;
            singleState.outputStep = stepCtx ? stepCtx->mOutputs.asObject<ExprMap>() : NULL;
            singleState.outputEnv = (ExprMap*)stepState.env->mSlots[REnv_Job];
            singleState.env = stepState.env;
            singleState.index = i;
            
            executor->mState = mState;
            executor->mTask = mTask;
            executor->mStepState = singleState;
            
            stepResult = executor->execute();
            delete executor;
         }
         
         
         if (!(stepResult.result == runner::v1::RESULT_SUCCESS || stepResult.result == runner::v1::RESULT_SKIPPED) &&
             !step->mContinueOnError.getBool())
         {
            jobResult = stepResult.result;
            mTask->endStep(jobResult);
            break;
         }
         
         sleep(1);
         mTask->endStep(stepResult.result);
         stepCount++;
      }
      
      // Set all job outputs according to job description
      if (stepState.jobOutput)
      {
         std::vector<std::string> outputKeys;
         stepState.jobOutput->extractKeys(outputKeys);
         for (std::string& key : outputKeys)
         {
            ExprValue val = stepState.jobOutput->getMapKey(key);
            if (val.isString())
            {
               std::string outputData = mState->substituteExpressions(val.getString());
               mTask->addOutput(key, outputData.c_str());
            }
         }
      }
      
      //
      mTask->setResult(jobResult);
      mTask->log("End of job reached", true);
      mTask->_waitForLogSync();
   }
};

void PerformTask(TaskTracker* currentTask)
{
   ExprState* exprState = currentTask->_getExprState();
   currentTask->beginJob();
   
   std::vector<std::string> jobList;
   currentTask->_getWorkflow()->mJobs.getObject()->extractKeys(jobList);
   
   std::string jobName = jobList[0];
   JobDefinition* jobDefinition = currentTask->_getWorkflow()->mJobs.asObject<ExprMap>()->getMapKey(jobName).asObject<JobDefinition>();
   
   const char* jobUses = jobDefinition->mUses.getString();
   if (jobUses == NULL)
   {
      jobUses = "default";
   }
   
   JobExecutor::CreateFunc executorFunc = JobExecutor::getExecutor(jobUses);
   if (executorFunc == NULL)
   {
      std::string err = std::string("Error: couldn't find executor: ") + jobUses;
      currentTask->setResult(runner::v1::RESULT_FAILURE);
      currentTask->log(err.c_str(), true);
      currentTask->_waitForLogSync();
   }
   else
   {
      JobExecutor* executor = executorFunc();
      executor->mState = exprState;
      executor->mJob = jobDefinition;
      executor->mJobName = jobName;
      executor->mTask = currentTask;
      executor->mCurrentStep = 0;
      executor->mNumSteps = 0;
      executor->handleJob();
      delete executor;
   }
   
   currentTask->endJob();
   currentTask->setFinished();
}

std::unordered_map<std::string, FuncInfo> ExprState::smFunctions;

// Entrypoint
int main(int argc, char** argv)
{
   // Initialize libcurl
   CURL *curl = curl_easy_init();
   if (!curl) {
      printf("Failed to initialize CURL\n");
      return 1;
   }
   
   ExprFieldObject::registerFieldsForType<BasicContext>();
   ExprFieldObject::registerFieldsForType<JobDefinition>();
   ExprFieldObject::registerFieldsForType<JobStepDefinition>();
   ExprFieldObject::registerFieldsForType<SingleWorkflowDefinition>();
   ExprFieldObject::registerFieldsForType<JobStepContext>();
   ExprFieldObject::registerFieldsForType<JobResultContext>();
   ExprFieldObject::registerFieldsForType<CurrentJobContext>();
   ExprFieldObject::registerFieldsForType<RunnerInfo>();
   
   JobExecutor::registerExecutor("default", JobExecutor::createExecutor<StandardJobExecutor>);
   JobStepExecutor::registerExecutor("default", JobStepExecutor::createExecutor<StandardStepExecutor>);
   
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
