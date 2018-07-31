// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: server.proto

#define INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION
#include "server.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/port.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)

namespace pb {

namespace {

const ::google::protobuf::Descriptor* G2gAllRoomFrameData_descriptor_ = NULL;
const ::google::protobuf::internal::GeneratedMessageReflection*
  G2gAllRoomFrameData_reflection_ = NULL;

}  // namespace


void protobuf_AssignDesc_server_2eproto() GOOGLE_ATTRIBUTE_COLD;
void protobuf_AssignDesc_server_2eproto() {
  protobuf_AddDesc_server_2eproto();
  const ::google::protobuf::FileDescriptor* file =
    ::google::protobuf::DescriptorPool::generated_pool()->FindFileByName(
      "server.proto");
  GOOGLE_CHECK(file != NULL);
  G2gAllRoomFrameData_descriptor_ = file->message_type(0);
  static const int G2gAllRoomFrameData_offsets_[2] = {
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(G2gAllRoomFrameData, frametime_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(G2gAllRoomFrameData, datalist_),
  };
  G2gAllRoomFrameData_reflection_ =
    ::google::protobuf::internal::GeneratedMessageReflection::NewGeneratedMessageReflection(
      G2gAllRoomFrameData_descriptor_,
      G2gAllRoomFrameData::default_instance_,
      G2gAllRoomFrameData_offsets_,
      -1,
      -1,
      -1,
      sizeof(G2gAllRoomFrameData),
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(G2gAllRoomFrameData, _internal_metadata_),
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(G2gAllRoomFrameData, _is_default_instance_));
}

namespace {

GOOGLE_PROTOBUF_DECLARE_ONCE(protobuf_AssignDescriptors_once_);
inline void protobuf_AssignDescriptorsOnce() {
  ::google::protobuf::GoogleOnceInit(&protobuf_AssignDescriptors_once_,
                 &protobuf_AssignDesc_server_2eproto);
}

void protobuf_RegisterTypes(const ::std::string&) GOOGLE_ATTRIBUTE_COLD;
void protobuf_RegisterTypes(const ::std::string&) {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedMessage(
      G2gAllRoomFrameData_descriptor_, &G2gAllRoomFrameData::default_instance());
}

}  // namespace

void protobuf_ShutdownFile_server_2eproto() {
  delete G2gAllRoomFrameData::default_instance_;
  delete G2gAllRoomFrameData_reflection_;
}

void protobuf_AddDesc_server_2eproto() GOOGLE_ATTRIBUTE_COLD;
void protobuf_AddDesc_server_2eproto() {
  static bool already_here = false;
  if (already_here) return;
  already_here = true;
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  ::pb::protobuf_AddDesc_login_2eproto();
  ::google::protobuf::DescriptorPool::InternalAddGeneratedFile(
    "\n\014server.proto\022\002pb\032\013login.proto\"Y\n\023G2gAl"
    "lRoomFrameData\022\021\n\tFrameTime\030\001 \001(\r\022/\n\010Dat"
    "alist\030\002 \003(\0132\035.pb.S2CServerFrameUpdate_20"
    "01B\005\252\002\002pbb\006proto3", 137);
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedFile(
    "server.proto", &protobuf_RegisterTypes);
  G2gAllRoomFrameData::default_instance_ = new G2gAllRoomFrameData();
  G2gAllRoomFrameData::default_instance_->InitAsDefaultInstance();
  ::google::protobuf::internal::OnShutdown(&protobuf_ShutdownFile_server_2eproto);
}

// Force AddDescriptors() to be called at static initialization time.
struct StaticDescriptorInitializer_server_2eproto {
  StaticDescriptorInitializer_server_2eproto() {
    protobuf_AddDesc_server_2eproto();
  }
} static_descriptor_initializer_server_2eproto_;

// ===================================================================

#if !defined(_MSC_VER) || _MSC_VER >= 1900
const int G2gAllRoomFrameData::kFrameTimeFieldNumber;
const int G2gAllRoomFrameData::kDatalistFieldNumber;
#endif  // !defined(_MSC_VER) || _MSC_VER >= 1900

G2gAllRoomFrameData::G2gAllRoomFrameData()
  : ::google::protobuf::Message(), _internal_metadata_(NULL) {
  SharedCtor();
  // @@protoc_insertion_point(constructor:pb.G2gAllRoomFrameData)
}

void G2gAllRoomFrameData::InitAsDefaultInstance() {
  _is_default_instance_ = true;
}

G2gAllRoomFrameData::G2gAllRoomFrameData(const G2gAllRoomFrameData& from)
  : ::google::protobuf::Message(),
    _internal_metadata_(NULL) {
  SharedCtor();
  MergeFrom(from);
  // @@protoc_insertion_point(copy_constructor:pb.G2gAllRoomFrameData)
}

void G2gAllRoomFrameData::SharedCtor() {
    _is_default_instance_ = false;
  _cached_size_ = 0;
  frametime_ = 0u;
}

G2gAllRoomFrameData::~G2gAllRoomFrameData() {
  // @@protoc_insertion_point(destructor:pb.G2gAllRoomFrameData)
  SharedDtor();
}

void G2gAllRoomFrameData::SharedDtor() {
  if (this != default_instance_) {
  }
}

void G2gAllRoomFrameData::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const ::google::protobuf::Descriptor* G2gAllRoomFrameData::descriptor() {
  protobuf_AssignDescriptorsOnce();
  return G2gAllRoomFrameData_descriptor_;
}

const G2gAllRoomFrameData& G2gAllRoomFrameData::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_server_2eproto();
  return *default_instance_;
}

G2gAllRoomFrameData* G2gAllRoomFrameData::default_instance_ = NULL;

G2gAllRoomFrameData* G2gAllRoomFrameData::New(::google::protobuf::Arena* arena) const {
  G2gAllRoomFrameData* n = new G2gAllRoomFrameData;
  if (arena != NULL) {
    arena->Own(n);
  }
  return n;
}

void G2gAllRoomFrameData::Clear() {
// @@protoc_insertion_point(message_clear_start:pb.G2gAllRoomFrameData)
  frametime_ = 0u;
  datalist_.Clear();
}

bool G2gAllRoomFrameData::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!GOOGLE_PREDICT_TRUE(EXPRESSION)) goto failure
  ::google::protobuf::uint32 tag;
  // @@protoc_insertion_point(parse_start:pb.G2gAllRoomFrameData)
  for (;;) {
    ::std::pair< ::google::protobuf::uint32, bool> p = input->ReadTagWithCutoff(127);
    tag = p.first;
    if (!p.second) goto handle_unusual;
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // optional uint32 FrameTime = 1;
      case 1: {
        if (tag == 8) {
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::uint32, ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>(
                 input, &frametime_)));

        } else {
          goto handle_unusual;
        }
        if (input->ExpectTag(18)) goto parse_Datalist;
        break;
      }

      // repeated .pb.S2CServerFrameUpdate_2001 Datalist = 2;
      case 2: {
        if (tag == 18) {
         parse_Datalist:
          DO_(input->IncrementRecursionDepth());
         parse_loop_Datalist:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtualNoRecursionDepth(
                input, add_datalist()));
        } else {
          goto handle_unusual;
        }
        if (input->ExpectTag(18)) goto parse_loop_Datalist;
        input->UnsafeDecrementRecursionDepth();
        if (input->ExpectAtEnd()) goto success;
        break;
      }

      default: {
      handle_unusual:
        if (tag == 0 ||
            ::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          goto success;
        }
        DO_(::google::protobuf::internal::WireFormatLite::SkipField(input, tag));
        break;
      }
    }
  }
success:
  // @@protoc_insertion_point(parse_success:pb.G2gAllRoomFrameData)
  return true;
failure:
  // @@protoc_insertion_point(parse_failure:pb.G2gAllRoomFrameData)
  return false;
#undef DO_
}

void G2gAllRoomFrameData::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // @@protoc_insertion_point(serialize_start:pb.G2gAllRoomFrameData)
  // optional uint32 FrameTime = 1;
  if (this->frametime() != 0) {
    ::google::protobuf::internal::WireFormatLite::WriteUInt32(1, this->frametime(), output);
  }

  // repeated .pb.S2CServerFrameUpdate_2001 Datalist = 2;
  for (unsigned int i = 0, n = this->datalist_size(); i < n; i++) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      2, this->datalist(i), output);
  }

  // @@protoc_insertion_point(serialize_end:pb.G2gAllRoomFrameData)
}

::google::protobuf::uint8* G2gAllRoomFrameData::InternalSerializeWithCachedSizesToArray(
    bool deterministic, ::google::protobuf::uint8* target) const {
  // @@protoc_insertion_point(serialize_to_array_start:pb.G2gAllRoomFrameData)
  // optional uint32 FrameTime = 1;
  if (this->frametime() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::WriteUInt32ToArray(1, this->frametime(), target);
  }

  // repeated .pb.S2CServerFrameUpdate_2001 Datalist = 2;
  for (unsigned int i = 0, n = this->datalist_size(); i < n; i++) {
    target = ::google::protobuf::internal::WireFormatLite::
      InternalWriteMessageNoVirtualToArray(
        2, this->datalist(i), false, target);
  }

  // @@protoc_insertion_point(serialize_to_array_end:pb.G2gAllRoomFrameData)
  return target;
}

int G2gAllRoomFrameData::ByteSize() const {
// @@protoc_insertion_point(message_byte_size_start:pb.G2gAllRoomFrameData)
  int total_size = 0;

  // optional uint32 FrameTime = 1;
  if (this->frametime() != 0) {
    total_size += 1 +
      ::google::protobuf::internal::WireFormatLite::UInt32Size(
        this->frametime());
  }

  // repeated .pb.S2CServerFrameUpdate_2001 Datalist = 2;
  total_size += 1 * this->datalist_size();
  for (int i = 0; i < this->datalist_size(); i++) {
    total_size +=
      ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
        this->datalist(i));
  }

  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void G2gAllRoomFrameData::MergeFrom(const ::google::protobuf::Message& from) {
// @@protoc_insertion_point(generalized_merge_from_start:pb.G2gAllRoomFrameData)
  if (GOOGLE_PREDICT_FALSE(&from == this)) {
    ::google::protobuf::internal::MergeFromFail(__FILE__, __LINE__);
  }
  const G2gAllRoomFrameData* source = 
      ::google::protobuf::internal::DynamicCastToGenerated<const G2gAllRoomFrameData>(
          &from);
  if (source == NULL) {
  // @@protoc_insertion_point(generalized_merge_from_cast_fail:pb.G2gAllRoomFrameData)
    ::google::protobuf::internal::ReflectionOps::Merge(from, this);
  } else {
  // @@protoc_insertion_point(generalized_merge_from_cast_success:pb.G2gAllRoomFrameData)
    MergeFrom(*source);
  }
}

void G2gAllRoomFrameData::MergeFrom(const G2gAllRoomFrameData& from) {
// @@protoc_insertion_point(class_specific_merge_from_start:pb.G2gAllRoomFrameData)
  if (GOOGLE_PREDICT_FALSE(&from == this)) {
    ::google::protobuf::internal::MergeFromFail(__FILE__, __LINE__);
  }
  datalist_.MergeFrom(from.datalist_);
  if (from.frametime() != 0) {
    set_frametime(from.frametime());
  }
}

void G2gAllRoomFrameData::CopyFrom(const ::google::protobuf::Message& from) {
// @@protoc_insertion_point(generalized_copy_from_start:pb.G2gAllRoomFrameData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

void G2gAllRoomFrameData::CopyFrom(const G2gAllRoomFrameData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:pb.G2gAllRoomFrameData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool G2gAllRoomFrameData::IsInitialized() const {

  return true;
}

void G2gAllRoomFrameData::Swap(G2gAllRoomFrameData* other) {
  if (other == this) return;
  InternalSwap(other);
}
void G2gAllRoomFrameData::InternalSwap(G2gAllRoomFrameData* other) {
  std::swap(frametime_, other->frametime_);
  datalist_.UnsafeArenaSwap(&other->datalist_);
  _internal_metadata_.Swap(&other->_internal_metadata_);
  std::swap(_cached_size_, other->_cached_size_);
}

::google::protobuf::Metadata G2gAllRoomFrameData::GetMetadata() const {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::Metadata metadata;
  metadata.descriptor = G2gAllRoomFrameData_descriptor_;
  metadata.reflection = G2gAllRoomFrameData_reflection_;
  return metadata;
}

#if PROTOBUF_INLINE_NOT_IN_HEADERS
// G2gAllRoomFrameData

// optional uint32 FrameTime = 1;
void G2gAllRoomFrameData::clear_frametime() {
  frametime_ = 0u;
}
 ::google::protobuf::uint32 G2gAllRoomFrameData::frametime() const {
  // @@protoc_insertion_point(field_get:pb.G2gAllRoomFrameData.FrameTime)
  return frametime_;
}
 void G2gAllRoomFrameData::set_frametime(::google::protobuf::uint32 value) {
  
  frametime_ = value;
  // @@protoc_insertion_point(field_set:pb.G2gAllRoomFrameData.FrameTime)
}

// repeated .pb.S2CServerFrameUpdate_2001 Datalist = 2;
int G2gAllRoomFrameData::datalist_size() const {
  return datalist_.size();
}
void G2gAllRoomFrameData::clear_datalist() {
  datalist_.Clear();
}
const ::pb::S2CServerFrameUpdate_2001& G2gAllRoomFrameData::datalist(int index) const {
  // @@protoc_insertion_point(field_get:pb.G2gAllRoomFrameData.Datalist)
  return datalist_.Get(index);
}
::pb::S2CServerFrameUpdate_2001* G2gAllRoomFrameData::mutable_datalist(int index) {
  // @@protoc_insertion_point(field_mutable:pb.G2gAllRoomFrameData.Datalist)
  return datalist_.Mutable(index);
}
::pb::S2CServerFrameUpdate_2001* G2gAllRoomFrameData::add_datalist() {
  // @@protoc_insertion_point(field_add:pb.G2gAllRoomFrameData.Datalist)
  return datalist_.Add();
}
::google::protobuf::RepeatedPtrField< ::pb::S2CServerFrameUpdate_2001 >*
G2gAllRoomFrameData::mutable_datalist() {
  // @@protoc_insertion_point(field_mutable_list:pb.G2gAllRoomFrameData.Datalist)
  return &datalist_;
}
const ::google::protobuf::RepeatedPtrField< ::pb::S2CServerFrameUpdate_2001 >&
G2gAllRoomFrameData::datalist() const {
  // @@protoc_insertion_point(field_list:pb.G2gAllRoomFrameData.Datalist)
  return datalist_;
}

#endif  // PROTOBUF_INLINE_NOT_IN_HEADERS

// @@protoc_insertion_point(namespace_scope)

}  // namespace pb

// @@protoc_insertion_point(global_scope)
