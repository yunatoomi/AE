// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#ifdef AE_ENABLE_VULKAN

# include "graphics/Vulkan/RenderGraph/VRenderGraph.h"
# include "graphics/Vulkan/VDevice.h"
# include "graphics/Vulkan/VEnumCast.h"
# include "graphics/Vulkan/VResourceManager.h"
# include "graphics/Vulkan/Resources/VLogicalRenderPass.h"
# include "graphics/Vulkan/Resources/VFramebuffer.h"
# include "graphics/Vulkan/Resources/VRenderPass.h"

# include "graphics/Private/EnumUtils.h"

# include "stl/Memory/LinearAllocator.h"
# include "stl/Containers/StructView.h"

# include "threading/Containers/IndexedPool.h"

namespace AE::Graphics
{
	enum class ExeOrderIndex : uint {
		Initial	= 0,
		Unknown = ~0u
	};
	
	class VBarrierManager;
}

# include "graphics/Vulkan/RenderGraph/VCommandBatch.h"
# include "graphics/Vulkan/RenderGraph/VCommandPool.h"
# include "graphics/Vulkan/RenderGraph/VBarrierManager.h"
# include "graphics/Vulkan/RenderGraph/VLocalBuffer.h"
# include "graphics/Vulkan/RenderGraph/VLocalImage.h"
# include "graphics/Vulkan/RenderGraph/VGraphicsContext.h"
# include "graphics/Vulkan/RenderGraph/VIndirectGraphicsContext.h"
# include "graphics/Vulkan/RenderGraph/VRenderContext.h"
# include "graphics/Vulkan/RenderGraph/VCmdBatchDepsManager.h"

namespace AE::Graphics
{
	
	struct VRenderGraph::BaseCmd
	{
		enum class EState : uint8_t {
			Initial,
			Incomplete,
			Complete,
			Pending,
		};

		EState					state;
		EQueueType				queue;
		uint16_t				inputCount;
		uint16_t				outputCount;
		GfxResourceID *			input;
		GfxResourceID *			output;
		BaseCmd **				inputCmd;
		char *					dbgName;

		Function<bool (GraphicsContext &)>	pass;
	};

	struct VRenderGraph::RenderCmd : BaseCmd
	{
		//RenderPassSetupFn_t		setup;
		//RenderPassDrawFn_t		draw;
	};

	struct VRenderGraph::GraphicsCmd : BaseCmd
	{
	};

	struct VRenderGraph::ComputeCmd : BaseCmd
	{
	};

	struct VRenderGraph::TransferCmd : BaseCmd
	{
	};
//-----------------------------------------------------------------------------



/*
=================================================
	constructor
=================================================
*/
	VRenderGraph::VRenderGraph (VResourceManager &resMngr) :
		_resMngr{ resMngr },
		_taskDepsMngr{ MakeShared<VCmdBatchDepsManager>() }
	{
		Threading::Scheduler().RegisterDependency< CmdBatchDep >( _taskDepsMngr );
	}
	
/*
=================================================
	destructor
=================================================
*/
	VRenderGraph::~VRenderGraph ()
	{
		EXLOCK( _drCheck );

		for (auto& ctx : _contexts)
		{
			CHECK( not ctx.direct );
			CHECK( not ctx.indirect );
		}

		Threading::Scheduler().UnregisterDependency< CmdBatchDep >();
	}

/*
=================================================
	Initialize
=================================================
*/
	bool VRenderGraph::Initialize ()
	{
		EXLOCK( _drCheck );

		for (size_t i = 0; i < _contexts.size(); ++i)
		{
			auto	q = _resMngr.GetDevice().GetQueue( EQueueType(i) );
			if ( not q )
				continue;

			_contexts[i].direct = MakeUnique<GraphicsContext>( _resMngr, q );
			CHECK_ERR( _contexts[i].direct->Create() );

			// TODO: create indirect ctx
		}

		return true;
	}
	
/*
=================================================
	Deinitialize
=================================================
*/
	void VRenderGraph::Deinitialize ()
	{
		EXLOCK( _drCheck );

		CHECK( WaitIdle() );

		_cmdBatchPool.Release( /*checkForAssigned*/false );

		for (auto& ctx : _contexts) {
			ctx.direct.reset();
			ctx.indirect.reset();
		}
	}
	
/*
=================================================
	GetPresentQueues
=================================================
*/
	EQueueMask  VRenderGraph::GetPresentQueues ()
	{
		SHAREDLOCK( _drCheck );
		return Default;	// TODO
	}

/*
=================================================
	_Add
=================================================
*/
	template <typename T>
	inline T*  VRenderGraph::_Add (EQueueType			queue,
								   VirtualResources_t	input,
								   VirtualResources_t	output,
								   StringView			dbgName)
	{
		// TODO: lock-free
		EXLOCK( _cmdGuard );

		auto*	cmd = _allocator.Alloc<T>();
		CHECK_ERR( cmd );

		cmd->state			= BaseCmd::EState::Initial;
		cmd->queue			= queue;
		cmd->inputCount		= uint16_t(input.size());
		cmd->outputCount	= uint16_t(output.size());
		cmd->input			= input.size()   ? _allocator.Alloc<GfxResourceID>( input.size() )  : null;
		cmd->output			= output.size()  ? _allocator.Alloc<GfxResourceID>( output.size() ) : null;
		cmd->inputCmd		= input.size()   ? _allocator.Alloc<BaseCmd*>( input.size() )       : null;
		cmd->dbgName		= dbgName.size() ? _allocator.Alloc<char>( dbgName.length()+1 )     : null;

		for (size_t i = 0; i < input.size(); ++i)
		{
			cmd->input[i] = input[i].first;

			if ( input[i].first.IsVirtual() )
				_resUsage[ input[i].first ] |= input[i].second;
		}
		for (size_t i = 0; i < output.size(); ++i)
		{
			cmd->output[i] = output[i].first;

			if ( output[i].first.IsVirtual() )
				_resUsage[ output[i].first ] |= output[i].second;
		}

		if ( cmd->dbgName and dbgName.size() )
			std::memcpy( cmd->dbgName, dbgName.data(), dbgName.length()+1 );

		//DEBUG_ONLY(
		if ( cmd->inputCmd )
			std::memset( cmd->inputCmd, 0xCD, input.size() * sizeof(*cmd->inputCmd) );
		//)

		for (auto&[id, usage] : output)
		{
			auto&	dst = _resWriteCmd[ id ];
			CHECK( dst == null );
			dst = cmd;
		}

		_commands.push_back( cmd );
		return cmd;
	}

/*
=================================================
	Add
=================================================
*/
	bool VRenderGraph::Add (EQueueType				queue,
							VirtualResources_t		input,
							VirtualResources_t		output,
							RenderPassSetupFn_t&&	setup,
							RenderPassDrawFn_t&&	draw,
							StringView				dbgName)
	{
		CHECK_ERR( queue == EQueueType::Graphics );
		SHAREDLOCK( _drCheck );

		auto*	cmd = _Add<RenderCmd>( queue, input, output, dbgName );
		CHECK_ERR( cmd );

		// TODO
		//PlacementNew<RenderPassSetupFn_t>( &cmd->setup, std::move(setup) );
		//PlacementNew<RenderPassFn_t>( &cmd->draw, std::move(draw) );
		
		PlacementNew< Function< void (GraphicsContext &) >>(
			&cmd->pass,
			[this, setup = std::move(setup), draw = std::move(draw), cmd] (GraphicsContext &ctx) -> bool
			{
				RenderPassDesc	rp_desc;
				setup( ctx, ArrayView{cmd->input, cmd->inputCount}, ArrayView{cmd->output, cmd->outputCount}, OUT rp_desc );

				auto*	logical_rp = _CreateLogicalPass( rp_desc );
				CHECK_ERR( logical_rp );
				CHECK_ERR( _CreateRenderPass( {logical_rp} ));

				RenderContext	rctx{ ctx, *logical_rp };
				draw( rctx, ArrayView{cmd->input, cmd->inputCount}, ArrayView{cmd->output, cmd->outputCount} );
				return true;
			});
		return true;
	}
	
/*
=================================================
	Add
=================================================
*/
	bool VRenderGraph::Add (EQueueType				queue,
							VirtualResources_t		input,
							VirtualResources_t		output,
							GraphicsCommandFn_t&&	pass,
							StringView				dbgName)
	{
		CHECK_ERR( queue == EQueueType::Graphics );
		SHAREDLOCK( _drCheck );

		auto*	cmd = _Add<GraphicsCmd>( queue, input, output, dbgName );
		CHECK_ERR( cmd );
		
		PlacementNew< Function< void (GraphicsContext &) >>(
			&cmd->pass,
			[fn = std::move(pass), cmd] (GraphicsContext &ctx)
			{
				fn( ctx, ArrayView{cmd->input, cmd->inputCount}, ArrayView{cmd->output, cmd->outputCount} );
				return true;
			});
		return true;
	}
	
/*
=================================================
	Add
=================================================
*/
	bool VRenderGraph::Add (EQueueType				queue,
							VirtualResources_t		input,
							VirtualResources_t		output,
							ComputeCommandFn_t&&	pass,
							StringView				dbgName)
	{
		CHECK_ERR( queue == EQueueType::Graphics or queue == EQueueType::AsyncCompute );
		SHAREDLOCK( _drCheck );

		auto*	cmd = _Add<ComputeCmd>( queue, input, output, dbgName );
		CHECK_ERR( cmd );
		
		PlacementNew< Function< void (GraphicsContext &) >>(
			&cmd->pass,
			[fn = std::move(pass), cmd] (GraphicsContext &ctx)
			{
				fn( ctx, ArrayView{cmd->input, cmd->inputCount}, ArrayView{cmd->output, cmd->outputCount} );
				return true;
			});
		return true;
	}
	
/*
=================================================
	Add
=================================================
*/
	bool VRenderGraph::Add (EQueueType				queue,
							VirtualResources_t		input,
							VirtualResources_t		output,
							TransferCommandFn_t&&	pass,
							StringView				dbgName)
	{
		SHAREDLOCK( _drCheck );

		auto*	cmd = _Add<TransferCmd>( queue, input, output, dbgName );
		CHECK_ERR( cmd );
		
		PlacementNew< Function< void (GraphicsContext &) >>(
			&cmd->pass,
			[fn = std::move(pass), cmd] (GraphicsContext &ctx)
			{
				fn( ctx, ArrayView{cmd->input, cmd->inputCount}, ArrayView{cmd->output, cmd->outputCount} );
				return true;
			});
		return true;
	}
	
/*
=================================================
	Submit
----
	TODO: optimize sorting?, multithreading execution
=================================================
*/
	CmdBatchID  VRenderGraph::Submit ()
	{
		SHAREDLOCK( _drCheck );
		EXLOCK( _cmdGuard );

		_ResolveDependencies();

		Array<BaseCmd*>		ordered;
		_SortCommands( OUT ordered );

		_MergeRenderPasses();

		CmdBatchID		batch_id;
		VCommandBatch*	batch;
		if ( _AcquireNextBatch( OUT batch_id, OUT batch ))
		{
			CHECK_ERR( _ExecuteCommands( ordered, batch, batch_id ));
			_RecycleCmdBatches();
		}

		// reset
		_commands.clear();
		_resUsage.clear();
		_resWriteCmd.clear();
		_allocator.Discard();

		return batch_id;
	}
	
/*
=================================================
	_ResolveDependencies
=================================================
*/
	void  VRenderGraph::_ResolveDependencies ()
	{
		for (auto& cmd : _commands)
		{
			bool	complete = true;

			for (size_t i = 0; i < cmd->inputCount; ++i)
			{
				auto	iter	= _resWriteCmd.find( cmd->input[i] );
				auto*	dep		= iter != _resWriteCmd.end() ? iter->second : null;

				cmd->inputCmd[i] = dep;
				complete &= (dep != null);
			}

			cmd->state = complete ? BaseCmd::EState::Complete : BaseCmd::EState::Incomplete;
		}
	}
	
/*
=================================================
	_SortCommands
=================================================
*/
	void  VRenderGraph::_SortCommands (OUT Array<BaseCmd*> &ordered)
	{
		for (; _commands.size();)
		{
			for (auto iter = _commands.begin(); iter != _commands.end();)
			{
				if ( (*iter)->state != BaseCmd::EState::Complete )
				{
					iter = _commands.erase( iter );
					continue;
				}

				bool	failed		= false;
				bool	complete	= true;

				for (size_t i = 0; i < (*iter)->inputCount; ++i)
				{
					auto*	dep = (*iter)->inputCmd[i];
					ASSERT( dep );

					failed	 |= (dep->state == BaseCmd::EState::Incomplete);
					complete &= (dep->state == BaseCmd::EState::Pending);
				}

				if ( complete )
				{
					(*iter)->state = BaseCmd::EState::Pending;
					ordered.push_back( *iter );
					iter = _commands.erase( iter );
					continue;
				}

				if ( failed )
				{
					(*iter)->state = BaseCmd::EState::Incomplete;
					iter = _commands.erase( iter );
					continue;
				}

				++iter;
			}
		}
	}
	
/*
=================================================
	_MergeRenderPasses
=================================================
*/
	void  VRenderGraph::_MergeRenderPasses ()
	{
		// TODO
	}
	
/*
=================================================
	_AcquireNextBatch
=================================================
*/
	bool  VRenderGraph::_AcquireNextBatch (OUT CmdBatchID &batchId, OUT VCommandBatch* &outBatch)
	{
		outBatch = null;

		for (uint i = 0; i < 10; ++i)
		{
			uint	index;
			if ( _cmdBatchPool.Assign( OUT index, [](VCommandBatch* ptr, uint) { PlacementNew<VCommandBatch>( ptr ); }) )
			{
				outBatch = &_cmdBatchPool[index];
				batchId  = CmdBatchID{ index, outBatch->Generation() };
				break;
			}
			
			std::this_thread::yield();
		}
		
		if ( not outBatch )
			RETURN_ERR( "cmdbatch pool overflow!" );

		outBatch->Initialize();
		return true;
	}

/*
=================================================
	_ExecuteCommands
=================================================
*/
	bool  VRenderGraph::_ExecuteCommands (ArrayView<BaseCmd*> ordered, VCommandBatch* batch, CmdBatchID batchId)
	{
		CHECK_ERR( _contexts[uint(EQueueType::Graphics)].direct->Begin( batch, batchId ));

		for (auto* cmd : ordered)
		{
			ASSERT( cmd->state == BaseCmd::EState::Pending );
			
			CHECK( cmd->pass( *_contexts[uint(EQueueType::Graphics)].direct ));

			/*
			ASSERT( cmd->queue < EQueueType::_Count );

			if ( prev_queue != cmd->queue )
			{
				for (auto& ctx : _contexts) {
					CHECK( ctx->Submit() );
				}
				_contexts[ uint(cmd->queue) ]->Begin( batch );
			}

			CHECK( cmd->pass( *_contexts[uint(cmd->queue)] ));
			*/
		}

		for (auto& ctx : _contexts)
		{
			if ( ctx.direct ) {
				CHECK_ERR( ctx.direct->Submit() );
			}
		}

		return true;
	}
	
/*
=================================================
	_RecycleCmdBatches
=================================================
*/
	void  VRenderGraph::_RecycleCmdBatches ()
	{
		for (auto iter = _submitted.begin(); iter != _submitted.end();)
		{
			auto&	batch = _cmdBatchPool[ iter->Index() ];

			if ( batch.Generation() != iter->Generation() )
			{
				iter = _submitted.erase( iter );
				continue;
			}

			if ( batch.OnComplete( _resMngr ))
			{
				_cmdBatchPool.Unassign( iter->Index() );
				iter = _submitted.erase( iter );
				continue;
			}

			++iter;
		}

		_taskDepsMngr->Update( *this );
	}

/*
=================================================
	Wait
=================================================
*/
	bool  VRenderGraph::Wait (ArrayView<CmdBatchID> batches)
	{
		SHAREDLOCK( _drCheck );

		for (auto& id : batches)
		{
			auto&	batch = _cmdBatchPool[ id.Index() ];

			if ( batch.Generation() != id.Generation() )
				continue;

			CHECK( batch.OnComplete( _resMngr ));
		}

		_RecycleCmdBatches();
		return true;
	}
	
/*
=================================================
	WaitIdle
=================================================
*/
	bool  VRenderGraph::WaitIdle ()
	{
		SHAREDLOCK( _drCheck );

		auto&	dev = _resMngr.GetDevice();
		VK_CHECK( dev.vkDeviceWaitIdle( dev.GetVkDevice() ));

		_RecycleCmdBatches();
		return true;
	}
	
/*
=================================================
	IsComplete
=================================================
*/
	bool  VRenderGraph::IsComplete (ArrayView<CmdBatchID> batches)
	{
		SHAREDLOCK( _drCheck );

		for (auto& id : batches)
		{
			auto&	batch = _cmdBatchPool[ id.Index() ];

			if ( id.Generation() != batch.Generation() )
				continue;

			if ( not batch.IsComplete() )
				return false;
		}

		return true;
	}

/*
=================================================
	_CreateLogicalPass
=================================================
*/
	VLogicalRenderPass*  VRenderGraph::_CreateLogicalPass (const RenderPassDesc &desc)
	{
		VLogicalRenderPass*	ptr = null;
		
		// allocate
		{
			EXLOCK( _cmdGuard );
			ptr = _allocator.Alloc< VLogicalRenderPass >();
			CHECK_ERR( ptr );
		}
		
		PlacementNew< VLogicalRenderPass >( ptr );

		CHECK_ERR( ptr->Create( _resMngr, desc ));
		return ptr;
	}
	
/*
=================================================
	_CreateRenderPass
=================================================
*/
	bool  VRenderGraph::_CreateRenderPass (ArrayView<VLogicalRenderPass*> passes)
	{
		RenderPassID	pass_id = _resMngr.CreateRenderPass( passes );
		CHECK_ERR( pass_id );

		StaticArray< Pair<VImageID, ImageViewDesc>, GraphicsConfig::MaxAttachments >	attachments;

		uint2	dimension;
		uint	layers			= 0;
		bool	initialized		= false;
		uint	viewport_count	= 0;

		for (auto* pass : passes)
		{
			if ( pass == passes.front() )
				viewport_count = uint(pass->GetViewports().size());
			else
				CHECK_ERR( viewport_count == pass->GetViewports().size() );


			for (auto&[name, rt] : pass->GetColorTargets())
			{
				auto&	dst = attachments[rt.index];
				
				if ( dst.first == Default )
				{
					// initialize attachment
					dst.first	= rt.imageId;
					dst.second	= rt.viewDesc;

					// compare dimension and layer count
					{
						auto&	desc = _resMngr.GetDescription( rt.imageId );

						if ( initialized )
						{
							CHECK_ERR( All( dimension == uint2{ desc.dimension.x, desc.dimension.y } ));
							CHECK_ERR( layers == rt.viewDesc.layerCount );
						}
						else
						{
							dimension	= uint2{ desc.dimension.x, desc.dimension.y };
							layers		= rt.viewDesc.layerCount;
						}
					}
				}
				else
				{
					CHECK_ERR( dst.first  == rt.imageId );
					CHECK_ERR( dst.second == rt.viewDesc );
				}
			}
		}

		VFramebufferID	framebuffer_id = _resMngr.CreateFramebuffer( attachments, pass_id, dimension, layers );
		CHECK_ERR( framebuffer_id );

		for (size_t i = 0; i < passes.size(); ++i)
		{
			CHECK_ERR( passes[i]->SetRenderPass( _resMngr, pass_id, uint(i), framebuffer_id ));
		}
		return true;
	}


}	// AE::Graphics

#endif	// AE_ENABLE_VULKAN
