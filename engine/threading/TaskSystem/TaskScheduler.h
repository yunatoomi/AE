// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Containers/ArrayView.h"
#include "stl/Containers/FixedArray.h"
#include "stl/Containers/NtStringView.h"
#include "stl/Algorithms/Cast.h"
#include "stl/Types/Noncopyable.h"

#include "threading/Primitives/SpinLock.h"

#include <chrono>

#ifdef AE_ENABLE_VTUNE_API
#	include <ittnotify.h>
#	define AE_VTUNE( ... )	__VA_ARGS__
#else
#	define AE_VTUNE( ... )
#endif

#if 1
#	define AE_SCHEDULER_PROFILING( ... )	__VA_ARGS__
#else
#	define AE_SCHEDULER_PROFILING( ... )
#endif

namespace AE::Threading
{
	using Nanoseconds	= std::chrono::nanoseconds;


	//
	// Async Task interface
	//
	
	using AsyncTask = SharedPtr< class IAsyncTask >;

	class IAsyncTask : public std::enable_shared_from_this< IAsyncTask >
	{
		friend class TaskScheduler;
		friend class IThread;
		
	// types
	public:
		enum class EStatus : uint
		{
			Pending,		// task added to queue and waiting until input dependencies complete
			InProgress,		// task was acquired 

			_Finished,
			Completed,		// successfully completed

			_Interropted,
			Canceled,		// task was externally canceled
			Failed,			// task has internal error and has been failed
		};

		enum class EThread : uint
		{
			Main,		// thread with window message loop
			Worker,
			Renderer,	// for opengl
			FileIO,
			Network,
			_Count
		};


	// variables
	private:
		Atomic<EStatus>		_status		{EStatus::Pending};
		const EThread		_threadType;
		Array<AsyncTask>	_dependsOn;


	// methods
	public:
		ND_ EThread	Type ()		const	{ return _threadType; }

		ND_ EStatus	Status ()			{ return _status.load( memory_order_relaxed ); }

		ND_ bool	IsInQueue ()		{ return Status() < EStatus::_Finished; }
		ND_ bool	IsFinished ()		{ return Status() > EStatus::_Finished; }
		ND_ bool	IsInterropted ()	{ return Status() > EStatus::_Interropted; }

	protected:
		IAsyncTask (EThread type);

		virtual void Run () = 0;
		virtual void OnCancel () {}

		bool _ForceCanceledState ();
		bool _SetCanceledState ();
		bool _SetFailedState ();
		bool _SetCompletedState ();

		ND_ virtual NtStringView  DbgName () const	{ return "unknown"; }
	};




	//
	// Thread interface
	//

	class IThread : public std::enable_shared_from_this< IThread >
	{
	// types
	public:
		using EThread = IAsyncTask::EThread;

	// interface
	public:
		virtual bool  Attach (uint uid) = 0;
		virtual void  Detach () = 0;

		ND_ virtual NtStringView  DbgName () const	{ return "thread"; }

	// helper functions
	protected:
		static void _RunTask (const AsyncTask &);
		static void _SetFailedState (const AsyncTask &);
		static void _SetCompletedState (const AsyncTask &);
	};



	//
	// Task Scheduler
	//

	class TaskScheduler final : public Noncopyable
	{
	// types
	private:
		struct _PerQueue
		{
			SpinLock			guard;
			Array<AsyncTask>	tasks;
		};

		template <size_t N>
		class _TaskQueue
		{
		// variables
		public:
			FixedArray< _PerQueue, N >	queues;

			AE_SCHEDULER_PROFILING(
				Atomic<uint64_t>	_stallTime	{0};	// Nanoseconds
				Atomic<uint64_t>	_workTime	{0};
			)

		// methods
		public:
			_TaskQueue ();

			void  Resize (size_t count);
		};

		using MainQueue_t		= _TaskQueue< 2 >;
		using RenderQueue_t		= _TaskQueue< 2 >;
		using WorkerQueue_t		= _TaskQueue< 16 >;
		using FileQueue_t		= _TaskQueue< 2 >;
		using NetworkQueue_t	= _TaskQueue< 3 >;
		using ThreadPtr			= SharedPtr< IThread >;
		
		using TimePoint_t		= std::chrono::high_resolution_clock::time_point;
		using EStatus			= IAsyncTask::EStatus;
		using EThread			= IAsyncTask::EThread;
		

	// variables
	private:
		MainQueue_t			_mainQueue;
		WorkerQueue_t		_workerQueue;
		RenderQueue_t		_renderQueue;
		FileQueue_t			_fileQueue;
		NetworkQueue_t		_networkQueue;

		Array<ThreadPtr>	_threads;
		
		AE_VTUNE(
			__itt_domain*	_vtuneDomain	= null;
		)

	// methods
	public:
		ND_ static TaskScheduler&  Instance ();

			bool Setup (size_t maxWorkerThreads);
			void Release ();
			
		AE_VTUNE(
			ND_ __itt_domain*	GetVTuneDomain () const	{ return _vtuneDomain; }
		)

	// thread api
			bool AddThread (const ThreadPtr &thread);

			bool ProcessTask (EThread type, uint seed);

		ND_ AsyncTask  PickUpTask (EThread type, uint seed);

	// task api
		template <typename T, typename ...Args>
		ND_ AsyncTask  Run (Array<AsyncTask> &&dependsOn, Args&& ...args);

		ND_ bool  Wait (ArrayView<AsyncTask> tasks, Nanoseconds timeout = Nanoseconds{30'000'000'000});

			bool  Cancel (const AsyncTask &task);

	private:
		TaskScheduler ();
		~TaskScheduler ();

		AsyncTask  _InsertTask (const AsyncTask &task, Array<AsyncTask> &&dependsOn);

		template <size_t N>
		void  _AddTask (_TaskQueue<N> &tq, const AsyncTask &task) const;

		template <size_t N>
		AsyncTask  _PickUpTask (_TaskQueue<N> &tq, uint seed) const;
		
		template <size_t N>
		bool  _ProcessTask (_TaskQueue<N> &tq, uint seed) const;

		template <size_t N>
		static void  _WriteProfilerStat (StringView name, const _TaskQueue<N> &tq);
	};

	
/*
=================================================
	Run
=================================================
*/
	template <typename T, typename ...Args>
	inline AsyncTask  TaskScheduler::Run (Array<AsyncTask> &&dependsOn, Args&& ...args)
	{
		STATIC_ASSERT( IsBaseOf< IAsyncTask, T > );
		return _InsertTask( MakeShared<T>( std::forward<Args>(args)... ), std::move(dependsOn) );
	}

/*
=================================================
	Scheduler
=================================================
*/
	ND_ inline TaskScheduler&  Scheduler ()
	{
		return TaskScheduler::Instance();
	}


}	// AE::Threading