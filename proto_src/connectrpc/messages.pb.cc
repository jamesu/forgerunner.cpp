// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: connectrpc/messages.proto
// Protobuf C++ Version: 5.29.3

#include "connectrpc/messages.pb.h"

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
namespace connectrpc {

inline constexpr ErrorDetailItem::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : type_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR ErrorDetailItem::ErrorDetailItem(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct ErrorDetailItemDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ErrorDetailItemDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ErrorDetailItemDefaultTypeInternal() {}
  union {
    ErrorDetailItem _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ErrorDetailItemDefaultTypeInternal _ErrorDetailItem_default_instance_;

inline constexpr ErrorDetail::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : details_{},
        code_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        message_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR ErrorDetail::ErrorDetail(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct ErrorDetailDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ErrorDetailDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ErrorDetailDefaultTypeInternal() {}
  union {
    ErrorDetail _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ErrorDetailDefaultTypeInternal _ErrorDetail_default_instance_;
}  // namespace connectrpc
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_connectrpc_2fmessages_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_connectrpc_2fmessages_2eproto = nullptr;
const ::uint32_t
    TableStruct_connectrpc_2fmessages_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetailItem, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetailItem, _impl_.type_),
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetailItem, _impl_.value_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetail, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetail, _impl_.code_),
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetail, _impl_.message_),
        PROTOBUF_FIELD_OFFSET(::connectrpc::ErrorDetail, _impl_.details_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::connectrpc::ErrorDetailItem)},
        {10, -1, -1, sizeof(::connectrpc::ErrorDetail)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::connectrpc::_ErrorDetailItem_default_instance_._instance,
    &::connectrpc::_ErrorDetail_default_instance_._instance,
};
const char descriptor_table_protodef_connectrpc_2fmessages_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\031connectrpc/messages.proto\022\nconnectrpc\""
    ".\n\017ErrorDetailItem\022\014\n\004type\030\001 \001(\t\022\r\n\005valu"
    "e\030\002 \001(\t\"Z\n\013ErrorDetail\022\014\n\004code\030\001 \001(\t\022\017\n\007"
    "message\030\002 \001(\t\022,\n\007details\030\003 \003(\0132\033.connect"
    "rpc.ErrorDetailItemb\006proto3"
};
static ::absl::once_flag descriptor_table_connectrpc_2fmessages_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_connectrpc_2fmessages_2eproto = {
    false,
    false,
    187,
    descriptor_table_protodef_connectrpc_2fmessages_2eproto,
    "connectrpc/messages.proto",
    &descriptor_table_connectrpc_2fmessages_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_connectrpc_2fmessages_2eproto::offsets,
    file_level_enum_descriptors_connectrpc_2fmessages_2eproto,
    file_level_service_descriptors_connectrpc_2fmessages_2eproto,
};
namespace connectrpc {
// ===================================================================

class ErrorDetailItem::_Internal {
 public:
};

ErrorDetailItem::ErrorDetailItem(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:connectrpc.ErrorDetailItem)
}
inline PROTOBUF_NDEBUG_INLINE ErrorDetailItem::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::connectrpc::ErrorDetailItem& from_msg)
      : type_(arena, from.type_),
        value_(arena, from.value_),
        _cached_size_{0} {}

ErrorDetailItem::ErrorDetailItem(
    ::google::protobuf::Arena* arena,
    const ErrorDetailItem& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  ErrorDetailItem* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:connectrpc.ErrorDetailItem)
}
inline PROTOBUF_NDEBUG_INLINE ErrorDetailItem::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : type_(arena),
        value_(arena),
        _cached_size_{0} {}

inline void ErrorDetailItem::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
ErrorDetailItem::~ErrorDetailItem() {
  // @@protoc_insertion_point(destructor:connectrpc.ErrorDetailItem)
  SharedDtor(*this);
}
inline void ErrorDetailItem::SharedDtor(MessageLite& self) {
  ErrorDetailItem& this_ = static_cast<ErrorDetailItem&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.type_.Destroy();
  this_._impl_.value_.Destroy();
  this_._impl_.~Impl_();
}

inline void* ErrorDetailItem::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) ErrorDetailItem(arena);
}
constexpr auto ErrorDetailItem::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::CopyInit(sizeof(ErrorDetailItem),
                                            alignof(ErrorDetailItem));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull ErrorDetailItem::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_ErrorDetailItem_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &ErrorDetailItem::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<ErrorDetailItem>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &ErrorDetailItem::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<ErrorDetailItem>(), &ErrorDetailItem::ByteSizeLong,
            &ErrorDetailItem::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(ErrorDetailItem, _impl_._cached_size_),
        false,
    },
    &ErrorDetailItem::kDescriptorMethods,
    &descriptor_table_connectrpc_2fmessages_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* ErrorDetailItem::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 44, 2> ErrorDetailItem::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::connectrpc::ErrorDetailItem>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string value = 2;
    {::_pbi::TcParser::FastUS1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(ErrorDetailItem, _impl_.value_)}},
    // string type = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(ErrorDetailItem, _impl_.type_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string type = 1;
    {PROTOBUF_FIELD_OFFSET(ErrorDetailItem, _impl_.type_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // string value = 2;
    {PROTOBUF_FIELD_OFFSET(ErrorDetailItem, _impl_.value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\32\4\5\0\0\0\0\0"
    "connectrpc.ErrorDetailItem"
    "type"
    "value"
  }},
};

PROTOBUF_NOINLINE void ErrorDetailItem::Clear() {
// @@protoc_insertion_point(message_clear_start:connectrpc.ErrorDetailItem)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.type_.ClearToEmpty();
  _impl_.value_.ClearToEmpty();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* ErrorDetailItem::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const ErrorDetailItem& this_ = static_cast<const ErrorDetailItem&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* ErrorDetailItem::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const ErrorDetailItem& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:connectrpc.ErrorDetailItem)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // string type = 1;
          if (!this_._internal_type().empty()) {
            const std::string& _s = this_._internal_type();
            ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "connectrpc.ErrorDetailItem.type");
            target = stream->WriteStringMaybeAliased(1, _s, target);
          }

          // string value = 2;
          if (!this_._internal_value().empty()) {
            const std::string& _s = this_._internal_value();
            ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "connectrpc.ErrorDetailItem.value");
            target = stream->WriteStringMaybeAliased(2, _s, target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:connectrpc.ErrorDetailItem)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t ErrorDetailItem::ByteSizeLong(const MessageLite& base) {
          const ErrorDetailItem& this_ = static_cast<const ErrorDetailItem&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t ErrorDetailItem::ByteSizeLong() const {
          const ErrorDetailItem& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:connectrpc.ErrorDetailItem)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // string type = 1;
            if (!this_._internal_type().empty()) {
              total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                              this_._internal_type());
            }
            // string value = 2;
            if (!this_._internal_value().empty()) {
              total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                              this_._internal_value());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void ErrorDetailItem::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<ErrorDetailItem*>(&to_msg);
  auto& from = static_cast<const ErrorDetailItem&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:connectrpc.ErrorDetailItem)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_type().empty()) {
    _this->_internal_set_type(from._internal_type());
  }
  if (!from._internal_value().empty()) {
    _this->_internal_set_value(from._internal_value());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void ErrorDetailItem::CopyFrom(const ErrorDetailItem& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:connectrpc.ErrorDetailItem)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void ErrorDetailItem::InternalSwap(ErrorDetailItem* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.type_, &other->_impl_.type_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.value_, &other->_impl_.value_, arena);
}

::google::protobuf::Metadata ErrorDetailItem::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class ErrorDetail::_Internal {
 public:
};

ErrorDetail::ErrorDetail(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:connectrpc.ErrorDetail)
}
inline PROTOBUF_NDEBUG_INLINE ErrorDetail::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::connectrpc::ErrorDetail& from_msg)
      : details_{visibility, arena, from.details_},
        code_(arena, from.code_),
        message_(arena, from.message_),
        _cached_size_{0} {}

ErrorDetail::ErrorDetail(
    ::google::protobuf::Arena* arena,
    const ErrorDetail& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  ErrorDetail* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:connectrpc.ErrorDetail)
}
inline PROTOBUF_NDEBUG_INLINE ErrorDetail::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : details_{visibility, arena},
        code_(arena),
        message_(arena),
        _cached_size_{0} {}

inline void ErrorDetail::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
ErrorDetail::~ErrorDetail() {
  // @@protoc_insertion_point(destructor:connectrpc.ErrorDetail)
  SharedDtor(*this);
}
inline void ErrorDetail::SharedDtor(MessageLite& self) {
  ErrorDetail& this_ = static_cast<ErrorDetail&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.code_.Destroy();
  this_._impl_.message_.Destroy();
  this_._impl_.~Impl_();
}

inline void* ErrorDetail::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) ErrorDetail(arena);
}
constexpr auto ErrorDetail::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.details_) +
          decltype(ErrorDetail::_impl_.details_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::CopyInit(
        sizeof(ErrorDetail), alignof(ErrorDetail), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&ErrorDetail::PlacementNew_,
                                 sizeof(ErrorDetail),
                                 alignof(ErrorDetail));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull ErrorDetail::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_ErrorDetail_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &ErrorDetail::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<ErrorDetail>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &ErrorDetail::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<ErrorDetail>(), &ErrorDetail::ByteSizeLong,
            &ErrorDetail::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_._cached_size_),
        false,
    },
    &ErrorDetail::kDescriptorMethods,
    &descriptor_table_connectrpc_2fmessages_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* ErrorDetail::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 1, 42, 2> ErrorDetail::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::connectrpc::ErrorDetail>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // string code = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.code_)}},
    // string message = 2;
    {::_pbi::TcParser::FastUS1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.message_)}},
    // repeated .connectrpc.ErrorDetailItem details = 3;
    {::_pbi::TcParser::FastMtR1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.details_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string code = 1;
    {PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.code_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // string message = 2;
    {PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.message_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // repeated .connectrpc.ErrorDetailItem details = 3;
    {PROTOBUF_FIELD_OFFSET(ErrorDetail, _impl_.details_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::connectrpc::ErrorDetailItem>()},
  }}, {{
    "\26\4\7\0\0\0\0\0"
    "connectrpc.ErrorDetail"
    "code"
    "message"
  }},
};

PROTOBUF_NOINLINE void ErrorDetail::Clear() {
// @@protoc_insertion_point(message_clear_start:connectrpc.ErrorDetail)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.details_.Clear();
  _impl_.code_.ClearToEmpty();
  _impl_.message_.ClearToEmpty();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* ErrorDetail::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const ErrorDetail& this_ = static_cast<const ErrorDetail&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* ErrorDetail::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const ErrorDetail& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:connectrpc.ErrorDetail)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // string code = 1;
          if (!this_._internal_code().empty()) {
            const std::string& _s = this_._internal_code();
            ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "connectrpc.ErrorDetail.code");
            target = stream->WriteStringMaybeAliased(1, _s, target);
          }

          // string message = 2;
          if (!this_._internal_message().empty()) {
            const std::string& _s = this_._internal_message();
            ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "connectrpc.ErrorDetail.message");
            target = stream->WriteStringMaybeAliased(2, _s, target);
          }

          // repeated .connectrpc.ErrorDetailItem details = 3;
          for (unsigned i = 0, n = static_cast<unsigned>(
                                   this_._internal_details_size());
               i < n; i++) {
            const auto& repfield = this_._internal_details().Get(i);
            target =
                ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
                    3, repfield, repfield.GetCachedSize(),
                    target, stream);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:connectrpc.ErrorDetail)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t ErrorDetail::ByteSizeLong(const MessageLite& base) {
          const ErrorDetail& this_ = static_cast<const ErrorDetail&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t ErrorDetail::ByteSizeLong() const {
          const ErrorDetail& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:connectrpc.ErrorDetail)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .connectrpc.ErrorDetailItem details = 3;
            {
              total_size += 1UL * this_._internal_details_size();
              for (const auto& msg : this_._internal_details()) {
                total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
              }
            }
          }
           {
            // string code = 1;
            if (!this_._internal_code().empty()) {
              total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                              this_._internal_code());
            }
            // string message = 2;
            if (!this_._internal_message().empty()) {
              total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                              this_._internal_message());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void ErrorDetail::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<ErrorDetail*>(&to_msg);
  auto& from = static_cast<const ErrorDetail&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:connectrpc.ErrorDetail)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_details()->MergeFrom(
      from._internal_details());
  if (!from._internal_code().empty()) {
    _this->_internal_set_code(from._internal_code());
  }
  if (!from._internal_message().empty()) {
    _this->_internal_set_message(from._internal_message());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void ErrorDetail::CopyFrom(const ErrorDetail& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:connectrpc.ErrorDetail)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void ErrorDetail::InternalSwap(ErrorDetail* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.details_.InternalSwap(&other->_impl_.details_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.code_, &other->_impl_.code_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.message_, &other->_impl_.message_, arena);
}

::google::protobuf::Metadata ErrorDetail::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// @@protoc_insertion_point(namespace_scope)
}  // namespace connectrpc
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::std::false_type
    _static_init2_ PROTOBUF_UNUSED =
        (::_pbi::AddDescriptors(&descriptor_table_connectrpc_2fmessages_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
