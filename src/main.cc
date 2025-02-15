// SPDX-License-Identifier: MIT

#include <iostream>
#include <stdint.h>
#include <chrono>
#include <thread>
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

using ExprValue = ExpressionEval::ExprValue;
using ExprObject = ExpressionEval::ExprObject;
using ExprArray = ExpressionEval::ExprArray;
using ExprMap = ExpressionEval::ExprMap;
using ExprFieldObject = ExpressionEval::ExprFieldObject;
using ExprMultiKey = ExpressionEval::ExprMultiKey;
using ExprState = ExpressionEval::ExprState;
using FuncInfo = ExpressionEval::FuncInfo;

using namespace google::protobuf::util;

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

struct JobStep;
struct SingleWorkflowContext;

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
   
   runner::v1::Runner mRunner;
   runner::v1::Task mTask;
   runner::v1::UpdateTaskRequest mNextTaskRequest;
   runner::v1::StepState* mNextStepState;
   
   std::vector<LogBatch*> mBatchQueue;
   LogBatch* mHeadLog;
   
   uint64_t mLastLogAck;
   uint64_t mLastTaskAck;
   
   ExprState* mExprState;
   SingleWorkflowContext* mWorkflow;
   
   std::mutex mMutex;
   std::mutex mJobInfoMutex;
   std::atomic<bool> mFinished;
   std::atomic<bool> mCancelled;
   
   TaskTracker(const runner::v1::Runner& runner, const runner::v1::Task& task, ExprState* state, SingleWorkflowContext* ctx)
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

struct SingleWorkflowContext : public ExprFieldObject
{
   ExprValue mName;
   ExprValue mOn;
   ExprValue mEnv;
   ExprValue mJobs;
   ExprValue mDefaults;
   
   SingleWorkflowContext(ExprState* state);
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<SingleWorkflowContext>(); }
};

template<> void ExprFieldObject::registerFieldsForType<SingleWorkflowContext>()
{
   registerField<SingleWorkflowContext>("name", offsetof(SingleWorkflowContext, mName), ExprValue::STRING);
   registerField<SingleWorkflowContext>("on", offsetof(SingleWorkflowContext, mOn), ExprValue::OBJECT);
   registerField<SingleWorkflowContext>("env", offsetof(SingleWorkflowContext, mEnv), ExprValue::OBJECT);
   registerField<SingleWorkflowContext>("jobs", offsetof(SingleWorkflowContext, mJobs), ExprValue::OBJECT, false);
   registerField<SingleWorkflowContext>("defaults", offsetof(SingleWorkflowContext, mDefaults), ExprValue::OBJECT);
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
   registerField<BasicContext>("env", offsetof(BasicContext, mEnv), ExprValue::OBJECT);
   registerField<BasicContext>("timeout-minutes", offsetof(BasicContext, mTimeoutMinutes), ExprValue::NUMBER);
   registerField<BasicContext>("continue-on-error", offsetof(BasicContext, mContinueOnError), ExprValue::BOOLEAN);
}

struct ContainerContext;
struct StrategyContext;
struct ServicesContext;
struct PermissionsContext;

struct JobContext : public BasicContext
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
   
   JobContext(ExprState* state);
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<JobContext>(); }
};

template<> void ExprFieldObject::registerFieldsForType<JobContext>()
{
   auto& parentReg = getFieldRegistry<JobContext::Parent>();
   getFieldRegistry<JobContext>().insert(parentReg.begin(), parentReg.end());
   registerField<JobContext>("container", offsetof(JobContext, mContainer), ExprValue::OBJECT);
   registerField<JobContext>("strategy", offsetof(JobContext, mStrategy), ExprValue::STRING);
   registerField<JobContext>("services", offsetof(JobContext, mServices), ExprValue::OBJECT);
   registerField<JobContext>("permissions", offsetof(JobContext, mPermissions), ExprValue::OBJECT);
   registerField<JobContext>("defaults", offsetof(JobContext, mDefaults), ExprValue::OBJECT);
   registerField<JobContext>("steps", offsetof(JobContext, mSteps), ExprValue::OBJECT, false);
   registerField<JobContext>("needs", offsetof(JobContext, mNeeds), ExprValue::OBJECT);
   registerField<JobContext>("runs-on", offsetof(JobContext, mRunsOn), ExprValue::STRING);
   registerField<JobContext>("concurrency-group", offsetof(JobContext, mConcurrencyGroup), ExprValue::STRING);
}

struct JobStep : public BasicContext
{
   typedef BasicContext Parent;
   
   ExprValue mRun;
   ExprValue mShell;
   ExprValue mCwd;
   
   JobStep(ExprState* state) : BasicContext(state)
   {
   }
   
   virtual ~JobStep() = default;
   
   virtual runner::v1::Result execute(TaskTracker* tracker)
   {
      std::string cmdToRun = mState->substituteExpressions(mRun.getStringSafe());
      char buffer[4096];
      snprintf(buffer, sizeof(buffer), "TODO: run %s", cmdToRun.c_str());
      tracker->log(buffer);
      return runner::v1::RESULT_SUCCESS;
   }
   
   std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() override { return getFieldRegistry<JobStep>(); }
};

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
   registerField<JobContext>("id", offsetof(RunnerInfo, mId), ExprValue::STRING);
   registerField<JobContext>("uuid", offsetof(RunnerInfo, mUUID), ExprValue::STRING);
   registerField<JobContext>("token", offsetof(RunnerInfo, mToken), ExprValue::STRING);
   registerField<JobContext>("name", offsetof(RunnerInfo, mName), ExprValue::STRING);
   registerField<JobContext>("status", offsetof(RunnerInfo, mStatus), ExprValue::STRING);
   registerField<JobContext>("labels", offsetof(RunnerInfo, mLabels), ExprValue::OBJECT, false);
   //
   registerField<JobContext>("os", offsetof(RunnerInfo, mOs), ExprValue::STRING);
   registerField<JobContext>("arch", offsetof(RunnerInfo, mArch), ExprValue::STRING);
   registerField<JobContext>("temp", offsetof(RunnerInfo, mTemp), ExprValue::STRING);
   registerField<JobContext>("tool_cache", offsetof(RunnerInfo, mToolCache), ExprValue::STRING);
   registerField<JobContext>("debug", offsetof(RunnerInfo, mDebug), ExprValue::BOOLEAN);
   registerField<JobContext>("environment", offsetof(RunnerInfo, mEnvironment), ExprValue::STRING);
}

template<> void ExprFieldObject::registerFieldsForType<JobStep>()
{
   auto& parentReg = getFieldRegistry<JobStep::Parent>();
   getFieldRegistry<JobStep>().insert(parentReg.begin(), parentReg.end());
   registerField<JobStep>("run", offsetof(JobStep, mRun), ExprValue::STRING);
   registerField<JobStep>("shell", offsetof(JobStep, mShell), ExprValue::STRING);
   registerField<JobStep>("cwd", offsetof(JobStep, mCwd), ExprValue::STRING);
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


SingleWorkflowContext::SingleWorkflowContext(ExprState* state) : ExprFieldObject(state)
{
   ExprMap* jobMap = new ExprMap(state);
   jobMap->mAddObjectFunc = ExprArray::addTypedObjectFunc<JobContext>;
   mJobs.setObject(jobMap);
}

JobContext::JobContext(ExprState* state) : BasicContext(state)
{
   ExprArray* jobArray = new ExprArray(state);
   jobArray->mAddObjectFunc = ExprArray::addTypedObjectFunc<JobStep>;
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
         std::vector<JobStep*> steps;

         if (!rspTask.task().has_workflow_payload())
         {
            return NULL;
         }
         
         ExprState* exprState = new ExprState();
         
         exprState->mStringTable = new ExpressionEval::StringTable();
         SingleWorkflowContext* workFlowContext = new SingleWorkflowContext(exprState);

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

         tracker = new TaskTracker(state.info, rspTask.task(), exprState, workFlowContext);
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

// Example thread to send dummy updates to the server
void PerformTask(TaskTracker* currentTask)
{
   char buffer[512];
   
   // SETUP
   currentTask->beginJob();
   
   std::vector<std::string> jobList;
   currentTask->mWorkflow->mJobs.getObject()->extractKeys(jobList);
   
   std::string jobName = jobList[0];
   JobContext* jobContext = currentTask->mWorkflow->mJobs.asObject<ExprMap>()->getMapKey(jobName).asObject<JobContext>();
   
   ExprState* exprState = currentTask->mExprState;
   ExprMultiKey* env = new ExprMultiKey(exprState);
   env->mSlots[0] = currentTask->mWorkflow->mEnv.getObject();
   env->mSlots[1] = jobContext->mEnv.getObject();
   
   ExprMap* needsMap = new ExprMap(currentTask->mExprState);
   for (auto itr : currentTask->mTask.needs())
   {
      ExprMap* outMap = ProtoKVToObject(*exprState, itr.second.outputs());
      ExprMap* baseMap = new ExprMap(exprState);
      baseMap->setMapKey("outputs", ExprValue().setObject(outMap));
      baseMap->setMapKey("result", ExprValue().setString(*exprState->mStringTable, RunnerResultToString(itr.second.result())));
      needsMap->setMapKey(itr.first, ExprValue().setObject(baseMap));
   }
   
   RunnerInfo* runnerInfo = new RunnerInfo(exprState);
   ExprMap* gitContext = ProtoStructToObject(*exprState, currentTask->mTask.context());
   
   SetRunnerInfoFromProto(*exprState, currentTask->mRunner, runnerInfo);
   
   // Setup context for job
   exprState->setContext("github", gitContext);
   exprState->setContext("env", env);
   exprState->setContext("vars", ProtoKVToObject(*exprState, currentTask->mTask.vars()));
   exprState->setContext("job", jobContext);
   exprState->setContext("jobs", currentTask->mWorkflow->mJobs.getObject());
   exprState->setContext("steps", jobContext->mSteps.getObject());
   exprState->setContext("runner", runnerInfo);
   exprState->setContext("secrets", ProtoKVToObject(*exprState, currentTask->mTask.secrets()));
   exprState->setContext("needs", needsMap);
   exprState->setContext("inputs", new ExprMap(exprState)); // NOTE: this might need to be set inside the job?
   // NOTE: no matrix info is sent down so these are placeholders for now
   exprState->setContext("strategy", new ExprMap(exprState));
   exprState->setContext("matrix", new ExprMap(exprState));
   
   // Check if job has "if"
   if (jobContext->mConditional.getString())
   {
      std::string conditional = jobContext->mConditional.getString();
      ExprValue conditionalValue = exprState->substituteSingleExpression(conditional);
      if (conditionalValue.getBool() == false)
      {
         // Skip job
         currentTask->setResult(runner::v1::RESULT_SKIPPED);
         currentTask->log("Job conditional test failed", true);
         currentTask->waitForLogSync();
         //
         currentTask->endJob();
         currentTask->setFinished();
         return;
      }
      else
      {
         currentTask->log("Job conditional test passed");
      }
   }
   
   std::vector<JobStep*> steps;
   getTypedObjectsFromArray<JobStep>(jobContext->mSteps.asObject<ExprArray>(), steps);
   // SETUP DONE
   
   snprintf(buffer, sizeof(buffer), "Performing %i tasks for job %s...", (int)steps.size(), jobName.c_str());
   currentTask->log(buffer);
  
   //getTypedObjectsFromArray
   uint32_t stepCount = 0;
   runner::v1::Result jobResult = runner::v1::RESULT_SUCCESS;
   
   for (JobStep* step : steps)
   {
      // Update context for step
      env->mSlots[2] = step->mEnv.getObject();
      exprState->setContext("inputs", gitContext->getMapKey("event").getObject()->getMapKey("inputs").getObject());
      
      currentTask->beginStep(stepCount);
      
      // Check if step has "if"
      if (step->mConditional.getString())
      {
         std::string conditional = step->mConditional.getString();
         ExprValue conditionalValue = exprState->substituteSingleExpression(conditional);
         if (conditionalValue.getBool() == false)
         {
            // Skip job
            currentTask->log("Step conditional test failed", true);
            currentTask->endStep(runner::v1::RESULT_SKIPPED);
            continue;
         }
         else
         {
            currentTask->log("Step conditional test passed");
         }
      }
      
      snprintf(buffer, sizeof(buffer), "Doing something in step %u...", stepCount);
      currentTask->log(buffer);
      runner::v1::Result result = step->execute(currentTask);
      
      if (!(result == runner::v1::RESULT_SUCCESS || result == runner::v1::RESULT_SKIPPED) &&
          !step->mContinueOnError.getBool())
      {
         jobResult = result;
         currentTask->endStep(result);
         break;
      }
      
      sleep(1);
      currentTask->endStep(result);
      stepCount++;
   }
   
   //
   currentTask->setResult(jobResult);
   currentTask->log("End of job reached", true);
   currentTask->waitForLogSync();
   //
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
   ExprFieldObject::registerFieldsForType<JobContext>();
   ExprFieldObject::registerFieldsForType<JobStep>();
   ExprFieldObject::registerFieldsForType<SingleWorkflowContext>();
   ExprFieldObject::registerFieldsForType<RunnerInfo>();
   
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
