#pragma once

#include <type_traits>
#include <assert.h>
#include "memory.h"

template <typename T>
class maybe_const
{
	template<typename... _Args>
	using _Constructible = typename std::enable_if<
		std::is_constructible<shared_ptr<const T>, _Args...>::value
	>::type;
public:
	// Default copy constructor
	maybe_const(const maybe_const&) noexcept = default;

	// Create an empty maybe_const
	constexpr maybe_const() noexcept { }

	// Create a new empty maybe_const from nullptr
	explicit maybe_const(std::nullptr_t) noexcept { }

	// Create a new maybe_const to own the given const pointer
	template<typename OT, typename = _Constructible<const OT*>>
	explicit maybe_const(const OT *g) : const_pointer(g) {}

	// Create a new maybe_const to own the given non-const pointer
	template<typename OT, typename = _Constructible<OT*>>
	explicit maybe_const(OT *g) : pointer(g), const_pointer(pointer) {}

	// Create a new maybe_const to share the given const pointer
	template<typename OT, typename = _Constructible<const shared_ptr<const OT>&>>
	explicit maybe_const(const shared_ptr<const OT> &g) : const_pointer(g) {}

	// Create a new maybe_const to share the given non-const pointer
	template<typename OT, typename = _Constructible<const shared_ptr<OT>&>>
	explicit maybe_const(const shared_ptr<OT> &g) : pointer(g), const_pointer(pointer) {}

	// Create a new maybe_const copy
	template<typename OT, typename = _Constructible<const shared_ptr<OT>&>>
	maybe_const(const maybe_const<OT> &g) : pointer(g.pointer), const_pointer(g.const_pointer) {}

	inline bool isConst() const { return !pointer; }

	const T * operator->() const noexcept {
		return const_pointer.get();
	}

	const T & operator*() const noexcept {
		return *const_pointer;
	}

	explicit operator bool() const {
		return (bool)const_pointer;
	}

	shared_ptr<const T> constptr() const noexcept {
		return const_pointer;
	}

	shared_ptr<T> ptr() const { 
		assert(!isConst()); 
		return pointer; 
	}

	void reset(std::nullptr_t = nullptr)
	{
		pointer.reset();
		const_pointer.reset();
	}

	// reset to own a non-const pointer
	template<typename OT, typename = _Constructible<OT*>>
	void reset(OT *g)
	{
		pointer.reset(g);
		const_pointer = pointer;
	}

	// reset to own a const pointer
	template<typename OT, typename = _Constructible<const OT*>>
	void reset(const OT *g)
	{
		pointer.reset();
		const_pointer.reset(g);
	}

	// reset to share a non-const pointer
	template<typename OT, typename = _Constructible<const shared_ptr<OT>&>>
	void reset(const shared_ptr<OT> &g)
	{
		pointer = g;
		const_pointer = pointer;
	}

	// reset to share a const pointer
	template<typename OT, typename = _Constructible<const shared_ptr<const OT>&>>
	void reset(const shared_ptr<const OT> &g)
	{
		pointer.reset();
		const_pointer = g;
	}

	shared_ptr<T> pointer;
	shared_ptr<const T> const_pointer;
};
