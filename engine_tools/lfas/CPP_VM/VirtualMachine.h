// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'
/*
	This virtual machine emulates C++ memory model that described in standard.
	https://en.cppreference.com/w/cpp/atomic/memory_order

	Can be used for testing lock-free algorithms.
*/

#pragma once

#include "stl/Types/Noncopyable.h"
#include "stl/Containers/ArrayView.h"
#include "stl/Math/Random.h"

#include "Utils/MemRanges.h"

#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>

// exclusive lock
#ifndef EXLOCK
#	define EXLOCK( _syncObj_ ) \
		std::unique_lock	AE_PRIVATE_UNITE_RAW( __scopeLock, __COUNTER__ ) { _syncObj_ }
#endif

// shared lock
#ifndef SHAREDLOCK
#	define SHAREDLOCK( _syncObj_ ) \
		std::shared_lock	AE_PRIVATE_UNITE_RAW( __sharedLock, __COUNTER__ ) { _syncObj_ }
#endif


namespace LFAS::CPP
{
	using SecondsF	= std::chrono::duration<float>;



	//
	// Virtual Machine
	//

	class VirtualMachine final : public Noncopyable
	{
	// types
	public:
		struct AtomicGlobalLock
		{
		private:
			std::mutex *	_lock	= null;	// global lock for all atomics
			bool			_locked	= false;

		public:
			AtomicGlobalLock () {}
			explicit AtomicGlobalLock (std::mutex *lock);
			AtomicGlobalLock (const AtomicGlobalLock &) = delete;
			AtomicGlobalLock (AtomicGlobalLock &&);
			~AtomicGlobalLock ();
		};


		class Script;
		using ScriptPtr = SharedPtr<Script>;
		using ScriptFn	= Function< void () >;
		

	private:
		struct AtomicInfo
		{
			bool		isDestroyed	= true;
		};
		using AtomicMap_t	= HashMap< const void*, AtomicInfo >;

		using ThreadID_t	= std::thread::id;
		using MemRanges		= MemRangesTempl< ThreadID_t >;

		struct StorageInfo
		{
			BytesU		size;
			MemRanges	ranges;
			bool		isDestroyed	= true;
		};
		using StorageMap_t	= HashMap< const void*, StorageInfo >;


	// variables
	private:
		std::mutex					_atomicGlobalGuard;		// used for Sequentially-consistent ordering

		std::shared_mutex			_atomicMapGuard;
		AtomicMap_t					_atomicMap;

		mutable std::shared_mutex	_storageMapGuard;
		StorageMap_t				_storageMap;

		std::mutex					_randomGuard;
		Random						_random;

		std::atomic<uint>			_scriptCounter	{0};


	// methods
	private:
		VirtualMachine ();
		~VirtualMachine ();

	public:
		template <typename FN>
		ND_ ScriptPtr  CreateScript (FN &&fn);
		template <typename FN>
		ND_ ScriptPtr  CreateScript (StringView name, FN &&fn);
		ND_ ScriptPtr  CreateScript (StringView name, ScriptFn &&fn);

		void RunParallel (ArrayView<ScriptPtr> scripts, SecondsF timeout);


	// memory order
		void ThreadFenceAcquire ();
		void ThreadFenceRelease ();
		void ThreadFenceAcquireRelease ();
		void ThreadFenceRelaxed ();

		void Yield ();

		void CheckForUncommitedChanges () const;


	// atomics
		void AtomicCreate (const void *ptr);
		void AtomicDestroy (const void *ptr);

		ND_ bool AtomicCompareExchangeWeakFalsePositive (const void *ptr);

		ND_ AtomicGlobalLock  GetAtomicGlobalLock (bool isSequentiallyConsistent);


	// memory ranges
		void StorageCreate (const void *ptr, BytesU size);
		void StorageDestroy (const void *ptr);
		void StorageReadAccess (const void *ptr, BytesU offset, BytesU size);
		void StorageWriteAccess (const void *ptr, BytesU offset, BytesU size);


	// instance
		static bool  CreateInstance ();
		static bool  DestroyInstance ();
		ND_ static VirtualMachine&  Instance ();
	};

	

	template <typename FN>
	inline VirtualMachine::ScriptPtr  VirtualMachine::CreateScript (StringView name, FN &&fn)
	{
		return CreateScript( name, ScriptFn{std::forward<FN>(fn)} );
	}
	
	template <typename FN>
	inline VirtualMachine::ScriptPtr  VirtualMachine::CreateScript (FN &&fn)
	{
		return CreateScript( Default, ScriptFn{std::forward<FN>(fn)} );
	}

}	// LFAS::CPP
