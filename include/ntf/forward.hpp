#ifndef NTF_FORWARD_HPP
#define NTF_FORWARD_HPP

#include <ntf/impl/macro.h>

extern "C" {

NTF_DEFINE_HANDLE(ntf_Arena);

} // extern "C"

namespace ntf {

class MsgException;

template<typename T>
class DefaultAlloc;

template<typename Alloc>
class AllocDelete;

template<size_t Size, size_t Align>
struct AlignedBuffer;

template<typename T, size_t Size, size_t Align>
class TypeBuffer;

template<typename T, size_t Size, size_t Align>
class TypeArrayBuffer;

class Arena;

template<typename T>
class ArenaAlloc;

template<typename T, typename Deleter>
class UniquePtr;

template<typename T, typename Deleter>
class UniqueArray;

class Optional;

template<typename T>
class Ref;

template<typename Signature>
class FnRef;

template<typename E>
class BadExpectedAccess;

template<>
class BadExpectedAccess<void>;

class BadOptionalAccess;

template<typename E>
class unexpected;

template<typename T, typename E>
class Expected;

template<typename E>
class Expected<void, E>;

template<typename T, size_t MaxElems>
class FixedFreelist;

template<typename Signature, size_t MaxSize, size_t MaxAlign>
class TrivFn;

template<typename... Fs>
struct OverloadFn;

template<typename Fn>
class DeferFn;

template<typename T, size_t Extent>
class Span;

template<size_t N, typename Char>
class StringBuf;

template<typename T, size_t N>
class InplaceVec;

class ThreadPool;

} // namespace ntf

#endif // NTF_FORWARD_HPP
