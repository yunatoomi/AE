// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Math/Vec.h"

namespace AE::Math
{

	//
	// Value Scale Template
	//

	struct ValueScaleTempl
	{
	private:
		template <typename T>
		ND_ static constexpr T  _Abs (T val)
		{
			return val < 0 ? -val : val;
		}

	public:
		template <typename T, int IntVal>
		struct Integer
		{
			static constexpr T	Value = T(IntVal);
		};

		template <typename Lhs, typename Rhs>
		struct Add;

		template <typename Lhs, typename Rhs>
		struct Sub;
		
		template <typename Lhs, typename Rhs>
		struct Mul;
		
		template <typename Lhs, typename Rhs>
		struct Div;
		
		template <typename S>
		struct Inverse;

		template <typename S, uint Power>
		struct Pow;
	};

	

	//
	// Add
	//
	template <typename Lhs, typename Rhs>
	struct ValueScaleTempl::Add
	{
		static constexpr auto	Value	= Min( Lhs::Value, Rhs::Value );

		template <typename T>
		static constexpr T  Get (T lhs, T rhs)
		{
			if constexpr( _Abs(Lhs::Value) < _Abs(Rhs::Value) )
				return lhs + rhs * T(Rhs::Value / Lhs::Value);
			else
				return lhs * T(Lhs::Value / Rhs::Value) + rhs;
		}
	};


	//
	// Sub
	//
	template <typename Lhs, typename Rhs>
	struct ValueScaleTempl::Sub
	{
		static constexpr auto	Value	= Min( Lhs::Value, Rhs::Value );

		template <typename T>
		static constexpr T  Get (T lhs, T rhs)
		{
			if constexpr( _Abs(Lhs::Value) < _Abs(Rhs::Value) )
				return lhs - rhs * T(Rhs::Value / Lhs::Value);
			else
				return lhs * T(Lhs::Value / Rhs::Value) - rhs;
		}
	};

		
	//
	// Mul
	//
	template <typename Lhs, typename Rhs>
	struct ValueScaleTempl::Mul
	{
		static constexpr auto	Value	= Lhs::Value * Rhs::Value;

		template <typename T>
		static constexpr T  Get (T lhs, T rhs)
		{
			return lhs * rhs;
		}
	};

	
	//
	// Div
	//
	template <typename Lhs, typename Rhs>
	struct ValueScaleTempl::Div
	{
		static constexpr auto	Value	= Lhs::Value / Rhs::Value;

		template <typename T>
		static constexpr T  Get (T lhs, T rhs)
		{
			return lhs / rhs;
		}
	};


	//
	// Inverse
	//
	template <typename S>
	struct ValueScaleTempl::Inverse
	{
		static constexpr auto	Value	= decltype(S::Value)(1) / S::Value;

		template <typename T>
		static constexpr T  Get (T val)
		{
			return T(1) / val;
		}
	};


	//
	// Pow
	//
	template <typename S, uint Power>
	struct ValueScaleTempl::Pow
	{
		static constexpr auto	Value	= S::Value * Pow<S, Power-1>::Value;

		template <typename T>
		static constexpr T  Get (T val)
		{
			return val * T(Pow<S, Power-1>::Value);
		}
	};
	
	template <typename S>
	struct ValueScaleTempl::Pow< S, 1 >
	{
		static constexpr auto	Value	= S::Value;

		template <typename T>
		static constexpr T  Get (T val)
		{
			return val;
		}
	};
	
	template <typename S>
	struct ValueScaleTempl::Pow< S, 0 >
	{};


}	// AE::Math
