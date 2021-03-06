// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Math/Math.h"
#include "stl/Math/Vec.h"

namespace AE::Math
{

	//
	// Radians
	//

	template <typename T>
	struct RadiansTempl
	{
		STATIC_ASSERT( IsScalar<T> and IsFloatPoint<T> );
		
	// types
	public:
		using Self		= RadiansTempl<T>;
		using Value_t	= T;


	// variables
	private:
		T		_value;


	// methods
	public:
		constexpr RadiansTempl () : _value{} {}
		constexpr explicit RadiansTempl (T val) : _value{val} {}
		
		constexpr RadiansTempl (const Self &) = default;
		constexpr RadiansTempl (Self &&) = default;

		ND_ constexpr explicit operator T ()					const	{ return _value; }

		ND_ constexpr static Self  Pi ()								{ return Self{T( 3.14159265358979323846 )}; }
		ND_ constexpr static T DegToRad ()								{ return T(0.01745329251994329576923690768489); }
		ND_ constexpr static T RadToDeg ()								{ return T(57.295779513082320876798154814105); }

			Self&  operator = (const Self &) = default;
			Self&  operator = (Self &&) = default;

		ND_ constexpr Self   operator - ()						const	{ return Self{ -_value }; }

			constexpr Self&  operator += (const Self rhs)				{ _value += rhs._value;  return *this; }
			constexpr Self&  operator -= (const Self rhs)				{ _value -= rhs._value;  return *this; }
			constexpr Self&  operator *= (const Self rhs)				{ _value *= rhs._value;  return *this; }
			constexpr Self&  operator /= (const Self rhs)				{ _value /= rhs._value;  return *this; }

			constexpr Self&  operator += (const T rhs)					{ _value += rhs;  return *this; }
			constexpr Self&  operator -= (const T rhs)					{ _value -= rhs;  return *this; }
			constexpr Self&  operator *= (const T rhs)					{ _value *= rhs;  return *this; }
			constexpr Self&  operator /= (const T rhs)					{ _value /= rhs;  return *this; }
			
		ND_ constexpr Self   operator + (const Self rhs)		const	{ return Self{ _value + rhs._value }; }
		ND_ constexpr Self   operator - (const Self rhs)		const	{ return Self{ _value - rhs._value }; }
		ND_ constexpr Self   operator * (const Self rhs)		const	{ return Self{ _value * rhs._value }; }
		ND_ constexpr Self   operator / (const Self rhs)		const	{ return Self{ _value / rhs._value }; }

		ND_ constexpr Self   operator + (const T rhs)			const	{ return Self{ _value + rhs }; }
		ND_ constexpr Self   operator - (const T rhs)			const	{ return Self{ _value - rhs }; }
		ND_ constexpr Self   operator * (const T rhs)			const	{ return Self{ _value * rhs }; }
		ND_ constexpr Self   operator / (const T rhs)			const	{ return Self{ _value / rhs }; }

		ND_ constexpr bool	operator == (const Self rhs)		const	{ return _value == rhs._value; }
		ND_ constexpr bool	operator != (const Self rhs)		const	{ return _value != rhs._value; }
		ND_ constexpr bool	operator >  (const Self rhs)		const	{ return _value >  rhs._value; }
		ND_ constexpr bool	operator <  (const Self rhs)		const	{ return _value <  rhs._value; }
		ND_ constexpr bool	operator >= (const Self rhs)		const	{ return _value >= rhs._value; }
		ND_ constexpr bool	operator <= (const Self rhs)		const	{ return _value <= rhs._value; }
		
		ND_ friend constexpr Self  operator + (T lhs, Self rhs)			{ return Self{ lhs + rhs._value }; }
		ND_ friend constexpr Self  operator - (T lhs, Self rhs)			{ return Self{ lhs - rhs._value }; }
		ND_ friend constexpr Self  operator * (T lhs, Self rhs)			{ return Self{ lhs * rhs._value }; }
		ND_ friend constexpr Self  operator / (T lhs, Self rhs)			{ return Self{ lhs / rhs._value }; }
	};


	using Rad		= RadiansTempl<float>;
	using RadiansF	= RadiansTempl<float>;
	using RadiansD	= RadiansTempl<double>;

	inline static constexpr Rad  Pi = Rad::Pi();
	
	ND_ constexpr Rad  operator "" _rad (long double value)			{ return Rad{ Rad::Value_t(value) }; }
	ND_ constexpr Rad  operator "" _rad (unsigned long long value)	{ return Rad{ Rad::Value_t(value) }; }
	
	ND_ constexpr Rad  operator "" _deg (long double value)			{ return Rad{ Rad::DegToRad() * Rad::Value_t(value) }; }
	ND_ constexpr Rad  operator "" _deg (unsigned long long value)	{ return Rad{ Rad::DegToRad() * Rad::Value_t(value) }; }


	template <typename T, int I>
	using RadianVec = Vec< RadiansTempl<T>, I >;
	

/*
=================================================
	Abs
=================================================
*/
	template <typename T>
	ND_ forceinline RadiansTempl<T>  Abs (const RadiansTempl<T>& x)
	{
		return RadiansTempl<T>{ std::abs( T(x) )};
	}

/*
=================================================
	Sin
=================================================
*/
	template <typename T>
	ND_ forceinline T  Sin (const RadiansTempl<T>& x)
	{
		return std::sin( T(x) );
	}
	
/*
=================================================
	Cos
=================================================
*/
	template <typename T>
	ND_ forceinline T  Cos (const RadiansTempl<T>& x)
	{
		return std::cos( T(x) );
	}
	
/*
=================================================
	ASin
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ASin (const T& x)
	{
		ASSERT( x >= T(-1) and x <= T(1) );

		return RadiansTempl<T>{std::asin( x )};
	}
	
/*
=================================================
	ACos
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ACos (const T& x)
	{
		ASSERT( x >= T(-1) and x <= T(1) );

		return RadiansTempl<T>{std::acos( x )};
	}
	
/*
=================================================
	SinH
=================================================
*/
	template <typename T>
	ND_ inline T  SinH (const RadiansTempl<T>& x)
	{
		return std::sinh( T(x) );
	}
	
/*
=================================================
	CosH
=================================================
*/
	template <typename T>
	ND_ inline T  CosH (const RadiansTempl<T>& x)
	{
		return std::cosh( T(x) );
	}
	
/*
=================================================
	ASinH
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ASinH (const T& x)
	{
		return RadiansTempl<T>( SignOrZero( x ) * Ln( x + Sqrt( (x*x) + T(1) ) ) );
	}
	
/*
=================================================
	ACosH
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ACosH (const T& x)
	{
		ASSERT( x >= T(1) );
		return RadiansTempl<T>{Ln( x + Sqrt( (x*x) - T(1) ) )};
	}
	
/*
=================================================
	Tan
=================================================
*/
	template <typename T>
	ND_ inline T  Tan (const RadiansTempl<T>& x)
	{
		return std::tan( T(x) );
	}
	
/*
=================================================
	CoTan
=================================================
*/
	template <typename T>
	ND_ inline T  CoTan (const RadiansTempl<T>& x)
	{
		return SafeDiv( T(1), Tan( x ), T(0) );
	}
	
/*
=================================================
	TanH
=================================================
*/
	template <typename T>
	ND_ inline T  TanH (const RadiansTempl<T>& x)
	{
		return std::tanh( T(x) );
	}
	
/*
=================================================
	CoTanH
=================================================
*/
	template <typename T>
	ND_ inline T  CoTanH (const RadiansTempl<T>& x)
	{
		return SafeDiv( T(1), TanH( x ), T(0) );
	}
	
/*
=================================================
	ATan
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ATan (const T& y_over_x)
	{
		return RadiansTempl<T>{std::atan( y_over_x )};
	}
	
/*
=================================================
	ATan
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ATan (const T& y, const T& x)
	{
		return RadiansTempl<T>{std::atan2( y, x )};
	}
	
/*
=================================================
	ACoTan
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ACoTan (const T& x)
	{
		return RadiansTempl<T>{SafeDiv( T(1), ATan( x ), T(0) )};
	}
	
/*
=================================================
	ATanH
=================================================
*
	template <typename T>
	ND_ inline Radians<T>  ATanH (const T& x)
	{
		ASSERT( x > T(-1) and x < T(1) );

		if ( Abs(x) == T(1) )	return Infinity<T>();	else
		if ( Abs(x) > T(1) )	return NaN<T>();		else
								return Ln( (T(1) + x) / (T(1) - x) ) / T(2);
	}
	
/*
=================================================
	ACoTanH
=================================================
*/
	template <typename T>
	ND_ inline RadiansTempl<T>  ACoTanH (const T& x)
	{
		return RadiansTempl<T>{SafeDiv( T(1), ATanH( x ), T(0) )};
	}
	

}	// AE::Math
