// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: runner/v1/services.proto
// Protobuf C++ Version: 5.29.3

#include "runner/v1/services.pb.h"

#include <algorithm>
#include <type_traits>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/generated_message_tctable_impl.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
namespace runner {
namespace v1 {
}  // namespace v1
}  // namespace runner
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_runner_2fv1_2fservices_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_runner_2fv1_2fservices_2eproto = nullptr;
const ::uint32_t TableStruct_runner_2fv1_2fservices_2eproto::offsets[1] = {};
static constexpr ::_pbi::MigrationSchema* schemas = nullptr;
static constexpr ::_pb::Message* const* file_default_instances = nullptr;
const char descriptor_table_protodef_runner_2fv1_2fservices_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\030runner/v1/services.proto\022\trunner.v1\032\030r"
    "unner/v1/messages.proto2\373\002\n\rRunnerServic"
    "e\022E\n\010Register\022\032.runner.v1.RegisterReques"
    "t\032\033.runner.v1.RegisterResponse\"\000\022B\n\007Decl"
    "are\022\031.runner.v1.DeclareRequest\032\032.runner."
    "v1.DeclareResponse\"\000\022H\n\tFetchTask\022\033.runn"
    "er.v1.FetchTaskRequest\032\034.runner.v1.Fetch"
    "TaskResponse\"\000\022K\n\nUpdateTask\022\034.runner.v1"
    ".UpdateTaskRequest\032\035.runner.v1.UpdateTas"
    "kResponse\"\000\022H\n\tUpdateLog\022\033.runner.v1.Upd"
    "ateLogRequest\032\034.runner.v1.UpdateLogRespo"
    "nse\"\000b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_runner_2fv1_2fservices_2eproto_deps[1] =
    {
        &::descriptor_table_runner_2fv1_2fmessages_2eproto,
};
static ::absl::once_flag descriptor_table_runner_2fv1_2fservices_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_runner_2fv1_2fservices_2eproto = {
    false,
    false,
    453,
    descriptor_table_protodef_runner_2fv1_2fservices_2eproto,
    "runner/v1/services.proto",
    &descriptor_table_runner_2fv1_2fservices_2eproto_once,
    descriptor_table_runner_2fv1_2fservices_2eproto_deps,
    1,
    0,
    schemas,
    file_default_instances,
    TableStruct_runner_2fv1_2fservices_2eproto::offsets,
    file_level_enum_descriptors_runner_2fv1_2fservices_2eproto,
    file_level_service_descriptors_runner_2fv1_2fservices_2eproto,
};
namespace runner {
namespace v1 {
// @@protoc_insertion_point(namespace_scope)
}  // namespace v1
}  // namespace runner
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::std::false_type
    _static_init2_ PROTOBUF_UNUSED =
        (::_pbi::AddDescriptors(&descriptor_table_runner_2fv1_2fservices_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
