// Copyright (c) 1999,2001,2003  
// Utrecht University (The Netherlands),
// ETH Zurich (Switzerland),
// INRIA Sophia-Antipolis (France),
// Max-Planck-Institute Saarbruecken (Germany),
// and Tel-Aviv University (Israel).  All rights reserved. 
//
// This file is part of CGAL (www.cgal.org); you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 3 of the License,
// or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// 
//
// Author(s)     : Stefan Schirra, Sylvain Pion

// This is an adaptation of CGAL's Handle_for.h for OpenSCAD
// It re-implements Handle_for using std::shared_ptr and its std::atomic 
// overloads to perform consistent reference counting across multiple threads

// Note: A compiler error will be issued if CGAL's Handle_for.h is included 
// before this file. This is to ensure the CGAL headers and classes use this 
// implementation. OpenSCAD includes this file via cgal.h.
 
#ifndef CGAL_HANDLE_FOR_ATOMIC_H
#define CGAL_HANDLE_FOR_ATOMIC_H

// issue an error if Handle_for is already included
#ifdef CGAL_HANDLE_FOR_H
#error "CGAL/Handle_for.h included before CGAL_Handle_for_atomic.h"
#endif

// define CGAL_HANDLE_FOR_H to override the default Handle_for.h
#define CGAL_HANDLE_FOR_H

#include <CGAL/config.h>
#include <boost/config.hpp>
#include <algorithm>
#include <cstddef>

#include "spinlock_pool_multi.h"

#if defined(BOOST_MSVC)
#  pragma warning(push)
#  pragma warning(disable:4345) // Avoid warning  http://msdn.microsoft.com/en-us/library/wewb47ee(VS.80).aspx
#endif
namespace CGAL {

template <class T>
class Handle_for
{
	std::shared_ptr<T> ptr_;

	class scoped_lock : public Handle_for_lock_pool::scoped_lock {
	public:
		scoped_lock(const std::shared_ptr<T> &p) : Handle_for_lock_pool::scoped_lock(p.get()) { }
		scoped_lock(const std::shared_ptr<T> &p1, const std::shared_ptr<T> &p2) : 
			Handle_for_lock_pool::scoped_lock(p1.get(), p2.get()) { }
	};

public:

    typedef T element_type;
    
    typedef std::ptrdiff_t Id_type ;

    Handle_for()
    {
		// init a new shared pointer
		ptr_ = std::make_shared<T>();
    }

	Handle_for(const Handle_for& h)
	{
		scoped_lock lock(h.ptr_);
#ifndef CGAL_HANDLE_FOR_NO_REFCOUNT
		ptr_ = h.ptr_;
#else
		ptr_ = std::make_shared<T>(*h.ptr_);
#endif
	}

    Handle_for(const element_type& t)
    {
		// make a shared pointer via t's copy constructor
		ptr_ = std::make_shared<T>(t);
    }

#ifndef CGAL_CFG_NO_CPP0X_RVALUE_REFERENCE
	Handle_for(Handle_for && h)
	{
		scoped_lock lock(h.ptr_);
		ptr_ = std::move(h.ptr_);
	}
	
    Handle_for(element_type && t)
    {
		// make a shared pointer via t's copy constructor
		ptr_ = std::make_shared<T>(std::forward<element_type>(t));
    }
#endif

#if !defined CGAL_CFG_NO_CPP0X_VARIADIC_TEMPLATES && !defined CGAL_CFG_NO_CPP0X_RVALUE_REFERENCE
	template < typename T1, typename T2, typename... Args >
	Handle_for(T1 && t1, T2 && t2, Args && ... args)
	{
		ptr_ = std::make_shared<T>(std::forward<T1>(t1), std::forward<T2>(t2), std::forward<Args>(args)...);
	}
#else
	template < typename T1, typename T2 >
	Handle_for(const T1& t1, const T2& t2)
	{
		ptr_ = std::make_shared<T>(t1, t2);
	}

	template < typename T1, typename T2, typename T3 >
	Handle_for(const T1& t1, const T2& t2, const T3& t3)
	{
		ptr_ = std::make_shared<T>(t1, t2, t3);
	}

	template < typename T1, typename T2, typename T3, typename T4 >
	Handle_for(const T1& t1, const T2& t2, const T3& t3, const T4& t4)
	{
		ptr_ = std::make_shared<T>(t1, t2, t3, t4);
	}
#endif // CGAL_CFG_NO_CPP0X_VARIADIC_TEMPLATES

    Handle_for&
    operator=(const Handle_for& h)
    {
		// copy h's shared pointer
		scoped_lock lock(ptr_, h.ptr_);
#ifndef CGAL_HANDLE_FOR_NO_REFCOUNT
		ptr_ = h.ptr_;
#else
		ptr_ = std::make_shared<T>(*h.ptr_);
#endif
        return *this;
    }

    Handle_for&
    operator=(const element_type &t)
    {
		// make a shared pointer via t's copy constructor
		//std::shared_ptr<T> tmp = std::make_shared<T>(t);
		scoped_lock lock(ptr_);
		*ptr_ = t;
        return *this;
    }

#ifndef CGAL_CFG_NO_CPP0X_RVALUE_REFERENCE
    Handle_for&
    operator=(Handle_for && h)
    {
		scoped_lock lock(ptr_, h.ptr_);
		ptr_ = std::move(h.ptr_);
        return *this;
    }

    Handle_for&
    operator=(element_type && t)
    {
		// make a shared pointer via t's copy constructor
		//std::shared_ptr<T> tmp = std::make_shared<T>(std::forward<element_type>(t));
		scoped_lock lock(ptr_);
		*ptr_ = std::move(t);
        return *this;
    }
#endif

    ~Handle_for()
    {
		ptr_.reset();
    }

    Id_type id() const { return Ptr() - static_cast<T const*>(0); }
    
    bool identical(const Handle_for& h) const { return Ptr() == h.Ptr(); }

    // Ptr() is the "public" access to the pointer to the object.
    // The non-const version asserts that the instance is not shared.
    const element_type *
    Ptr() const
    {
       return ptr_.get();
    }

    bool
    is_shared() const
    {
		return ptr_.use_count() != 1;
    }

    bool
    unique() const
    {
		return ptr_.use_count() == 1;
    }

    long
    use_count() const
    {
		return ptr_.use_count();
    }

    void
    swap(Handle_for& h)
    {
		scoped_lock lock(ptr_, h.ptr_);
		std::shared_ptr<T> tmp = h.ptr_;
		h.ptr_ = ptr_;
		ptr_ = tmp;
    }

protected:

    void
    copy_on_write()
    {
		scoped_lock lock(ptr_);
		if (ptr_ && is_shared())
		{
			// make a new copy of the data via its copy constructor
			ptr_ = std::make_shared<T>(*ptr_.get());
		}
    }

    // ptr() is the protected access to the pointer.  Both const and non-const.
    // Redundant with Ptr().
    element_type *
	ptr()
	{
		//copy_on_write();
		return ptr_.get();
	}

    const element_type *
	ptr() const
	{
		return ptr_.get();
	}
};

template <class T>
inline
void
swap(Handle_for<T> &h1, Handle_for<T> &h2)
{
    h1.swap(h2);
}

template <class T>
inline
bool
identical(const Handle_for<T> &h1,
          const Handle_for<T> &h2)
{
    return h1.identical(h2);
}

template <class T> inline bool identical(const T &t1, const T &t2) { return &t1 == &t2; }

template <class T>
inline
const T&
get_pointee_or_identity(const Handle_for<T> &h)
{
	return *(h.Ptr());
}

template <class T>
inline
const T&
get_pointee_or_identity(const T &t)
{
	return t;
}

template <class T>
inline
const T&
get(const Handle_for<T> &h)
{
	return *(h.Ptr());
}

template <class T>
inline
const T&
get(const T &t)
{
	return t;
}

} //namespace CGAL

#if defined(BOOST_MSVC)
#  pragma warning(pop)
#endif

#endif // CGAL_HANDLE_FOR_ATOMIC_H
