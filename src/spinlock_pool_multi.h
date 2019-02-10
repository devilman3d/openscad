#pragma once

#ifndef spinmultilock_pool_h
#define spinmultilock_pool_h

#include <CGAL/basic.h>

#include <boost/smart_ptr/detail/spinlock.hpp>
#include <boost/smart_ptr/detail/spinlock_pool.hpp>
#include <boost/thread/tss.hpp>

#include <list>
#include <string>
#include <sstream>
#include <thread>

#include "Profile_counterx.h"

#if defined(BOOST_MSVC)
#  pragma warning(push)
#  pragma warning(disable:4146)
// warning on - applied on unsigned number
#endif

namespace CGAL {
	// A custom spinlock pool class to provide locking on multiple addresses.
	// This is based on boost's spinlock_pool class.
	// The template parameters are:
	//	M = The unique pool index: 0 = reserved for Gmpqx, 1 = reserved for Handle_for
	//	N = The number of spinlocks in the pool. This [plus G] can affect cache usage?!?!
	//	G = The number of extra global/static locks for the pool which can be accessed by 
	//      index. All other locks are modulated across the pool: address % N.
	template <size_t M, size_t N = 41, size_t G = 0>
	class spinlock_pool_multi {
		// a value indicating the NULL lock index
		static const size_t nulllock = size_t(-1);
		// the pool of spinlocks
		static boost::detail::spinlock pool_[N + G];
		// a thread-local "stack" of locks to provide thread-safe recursion
		static boost::thread_specific_ptr<std::list<size_t>> lock_stack;

		// gets the index of the spinlock for the given address
		template <class T>
		static size_t spinlock_index_for(const T *pv) {
			//assert(pv && "Taking spinlock for NULL");
#if defined(__VMS) && __INITIAL_POINTER_SIZE == 64  
			size_t result = reinterpret_cast<unsigned long long>(pv) >> (alignof(T) + 1);
#else  
			size_t result = reinterpret_cast<std::size_t>(pv) >> (alignof(T) + 1);
#endif
			return (result % N) + G;
		}

		// gets the spinlock for the given index
		static boost::detail::spinlock *spinlock_for_index(size_t si) {
			CGAL_assertion(si < N + G && "Spinlock index out of range");
			return &pool_[si];
		}

		// initialize the thread-local data (lock_stack)
		static void init_tls(){
			if (!lock_stack.get())
				lock_stack.reset(new std::list<size_t>());
		}

		// finalize the thread-local data (lock_stack)
		static void end_tls() {
			if (auto s = lock_stack.get()) {
				if (s->empty()) {
					delete s;
					lock_stack.release();
				}
			}
		}

		// check if the given spinlock index is on the stack
		static bool is_on_stack(size_t si) {
			for (auto ss : *lock_stack)
				if (ss == si)
					return true;
			return false;
		}

		// get a formatted string representing this pool
		static std::string SPM_COUNTER(const char *str) {
			std::stringstream ss;
			ss << "[spinlock_pool_multi<" << M << ">::" << str << "]";
			return ss.str();
		}

		// locks the spinlock for the given address and pushes it onto the stack
		static size_t push_lock(size_t si) {
			if (si != nulllock) {
				CGAL_TIME_PROFILER(SPM_COUNTER("wait time"), waitTime);
				CGAL_BRANCH_PROFILER(SPM_COUNTER("recursed/locks"), recursed);
				CGAL_BRANCH_PROFILER(SPM_COUNTER("contended/locks"), contended);
				CGAL_HISTOGRAM_PROFILER(SPM_COUNTER("index"), (unsigned)si);
				if (!is_on_stack(si)) {
					boost::detail::spinlock *sp = spinlock_for_index(si);
					CGAL_assertion(sp && "Locking NULL spinlock");
					unsigned k = 0;
					for (; !sp->try_lock(); )
						boost::detail::yield(k++);
					if (k != 0) {
						CGAL_BRANCH_PROFILER_BRANCH(contended);
					}
					CGAL_HISTOGRAM_PROFILER(SPM_COUNTER("spins"), k);
				}
				else {
					si = nulllock;
					CGAL_BRANCH_PROFILER_BRANCH(recursed);
				}
			}
			lock_stack->push_back(si);
			return si;
		}

		// pops the spinlock for the given address and unlocks it
		// it better be the next one on the stack or bad things will happen!!!
		static void pop_unlock(size_t si) {
			CGAL_assertion(lock_stack->back() == si && "Unlocking wrong spinlock");
			lock_stack->pop_back();
			if (si != nulllock) {
				boost::detail::spinlock *sp = spinlock_for_index(si);
				CGAL_assertion(sp && "Unlocking NULL spinlock");
				sp->unlock();
			}
		}
	public:
		// gets a spinlock from the pool for the given address
		template <class T>
		static boost::detail::spinlock *spinlock_for(const T *pv) {
			return spinlock_for_index(spinlock_index_for(pv));
		}

		// An RAII class to take scoped locks on multiple addresses.
		class scoped_lock {
			// the locks held by this instance
			size_t locks[3];
			size_t count;

			// don't allow copying
			scoped_lock(const scoped_lock &) = delete;
			scoped_lock &operator=(const scoped_lock &) = delete;
		public:
			template <class T>
			static void assert_locked(const T *q) {
				init_tls();
				size_t si = spinlock_index_for(q);
				CGAL_assertion(is_on_stack(si) && "Address is NOT locked");
			}

			template <class T1, class T2>
			static void assert_locked(const T1 *q1, const T2 *q2) {
				init_tls();
				size_t si1 = spinlock_index_for(q1);
				size_t si2 = spinlock_index_for(q2);
				CGAL_assertion(is_on_stack(si1) && is_on_stack(si2) && "Addresses are NOT locked");
			}

			// Creates a lock for the global lock at the index
			scoped_lock(size_t si) : count(1) {
				init_tls();
				CGAL_assertion(si < G && "Locking outside the static spinlock range");
				locks[0] = push_lock(si);
			}

			// Creates a lock for one address
			template <class T>
			scoped_lock(const T *q) : count(1) {
				init_tls();
				size_t si = spinlock_index_for(q);
				locks[0] = push_lock(si);
			}

			// Creates a lock for two addresses
			template <class T1, class T2>
			scoped_lock(const T1 *q1, const T2 *q2) : count(2) {
				init_tls();
				size_t si1 = spinlock_index_for(q1);
				size_t si2 = spinlock_index_for(q2);
				// sort to prevent deadlocks
				if (si1 > si2) std::swap(si1, si2);
				locks[0] = push_lock(si1);
				locks[1] = push_lock(si2);
			}

			// Creates a lock for three addresses
			template <class T1, class T2, class T3>
			scoped_lock(const T1 *q1, const T2 *q2, const T3 *q3) : count(3) {
				init_tls();
				size_t si1 = spinlock_index_for(q1);
				size_t si2 = spinlock_index_for(q2);
				size_t si3 = spinlock_index_for(q3);
				// sort to prevent deadlocks
				if (si1 > si2) std::swap(si1, si2);
				if (si1 > si3) std::swap(si1, si3);
				if (si2 > si3) std::swap(si2, si3);
				locks[0] = push_lock(si1);
				locks[1] = push_lock(si2);
				locks[2] = push_lock(si3);
			}

			// Destructor unlocks all locks
			~scoped_lock() {
				CGAL_HISTOGRAM_PROFILER(SPM_COUNTER("count"), (unsigned)count);
				CGAL_HISTOGRAM_PROFILER(SPM_COUNTER("depth"), (unsigned)lock_stack->size());
				if (count > 2) pop_unlock(locks[2]);
				if (count > 1) pop_unlock(locks[1]);
				if (count > 0) pop_unlock(locks[0]);
				end_tls();
			}
		};
	};

	template <size_t M, size_t N, size_t G>
	boost::detail::spinlock spinlock_pool_multi<M, N, G>::pool_[N + G] = { BOOST_DETAIL_SPINLOCK_INIT };

	template <size_t M, size_t N, size_t G>
	boost::thread_specific_ptr<std::list<size_t>> spinlock_pool_multi<M, N, G>::lock_stack;

	// the reserved spinlock pool for Gmp operations
	typedef spinlock_pool_multi<0> Gmp_lock_pool;
	// the scoped_lock type for Gmp spinlock pooled operations
	typedef Gmp_lock_pool::scoped_lock Gmp_lock;

	// the reserved spinlock pool for Handle_for operations
	typedef spinlock_pool_multi<1> Handle_for_lock_pool;
}

#endif // spinmultilock_pool_h
