#pragma once
//#include <xmmintrin.h>
#define VectorAligned	D3DXVECTOR3
#define Vector4D	D3DXVECTOR4
#define Vector2D	D3DXVECTOR2
#define Vector		D3DXVECTOR3
#define Quaternion	D3DXVECTOR4
#define RadianEuler	D3DXVECTOR3
#define Assert assert
// little-endian "IDST"
#define IDSTUDIOHEADER			(('T'<<24)+('S'<<16)+('D'<<8)+'I')
// little-endian "IDAG"
#define IDSTUDIOANIMGROUPHEADER	(('G'<<24)+('A'<<16)+('D'<<8)+'I')


typedef float vec_t;
struct Quaternion48
{
	unsigned short x:16;
	unsigned short y:16;
	unsigned short z:15;
	unsigned short wneg:1;
};
class Vector48
{
	unsigned short x:16;
	unsigned short y:16;
	unsigned short z:16;
};
struct matrix3x4_t
{
	float m_flMatVal[3][4];
};

const int float32bias = 127;
const int float16bias = 15;

const float maxfloat16bits = 65504.0f;

class float16
{
public:
	//float16() {}
	//float16( float f ) { m_storage.rawWord = ConvertFloatTo16bits(f); }

	void Init() { m_storage.rawWord = 0; }
	//	float16& operator=(const float16 &other) { m_storage.rawWord = other.m_storage.rawWord; return *this; }
	//	float16& operator=(const float &other) { m_storage.rawWord = ConvertFloatTo16bits(other); return *this; }
	//	operator unsigned short () { return m_storage.rawWord; }
	//	operator float () { return Convert16bitFloatTo32bits( m_storage.rawWord ); }
	unsigned short GetBits() const 
	{ 
		return m_storage.rawWord; 
	}
	float GetFloat() const 
	{ 
		return Convert16bitFloatTo32bits( m_storage.rawWord ); 
	}
	void SetFloat( float in ) 
	{ 
		m_storage.rawWord = ConvertFloatTo16bits( in ); 
	}

	bool IsInfinity() const
	{
		return m_storage.bits.biased_exponent == 31 && m_storage.bits.mantissa == 0;
	}
	bool IsNaN() const
	{
		return m_storage.bits.biased_exponent == 31 && m_storage.bits.mantissa != 0;
	}

	bool operator==(const float16 other) const { return m_storage.rawWord == other.m_storage.rawWord; }
	bool operator!=(const float16 other) const { return m_storage.rawWord != other.m_storage.rawWord; }

	//	bool operator< (const float other) const	   { return GetFloat() < other; }
	//	bool operator> (const float other) const	   { return GetFloat() > other; }

protected:
	union float32bits
	{
		float rawFloat;
		struct 
		{
			unsigned int mantissa : 23;
			unsigned int biased_exponent : 8;
			unsigned int sign : 1;
		} bits;
	};

	union float16bits
	{
		unsigned short rawWord;
		struct
		{
			unsigned short mantissa : 10;
			unsigned short biased_exponent : 5;
			unsigned short sign : 1;
		} bits;
	};

	static bool IsNaN( float16bits in )
	{
		return in.bits.biased_exponent == 31 && in.bits.mantissa != 0;
	}
	static bool IsInfinity( float16bits in )
	{
		return in.bits.biased_exponent == 31 && in.bits.mantissa == 0;
	}

	// 0x0001 - 0x03ff
	static unsigned short ConvertFloatTo16bits( float input )
	{
		if ( input > maxfloat16bits )
			input = maxfloat16bits;
		else if ( input < -maxfloat16bits )
			input = -maxfloat16bits;

		float16bits output;
		float32bits inFloat;

		inFloat.rawFloat = input;

		output.bits.sign = inFloat.bits.sign;

		if ( (inFloat.bits.biased_exponent==0) && (inFloat.bits.mantissa==0) ) 
		{ 
			// zero
			output.bits.mantissa = 0;
			output.bits.biased_exponent = 0;
		}
		else if ( (inFloat.bits.biased_exponent==0) && (inFloat.bits.mantissa!=0) ) 
		{  
			// denorm -- denorm float maps to 0 half
			output.bits.mantissa = 0;
			output.bits.biased_exponent = 0;
		}
		else if ( (inFloat.bits.biased_exponent==0xff) && (inFloat.bits.mantissa==0) ) 
		{ 
#if 0
			// infinity
			output.bits.mantissa = 0;
			output.bits.biased_exponent = 31;
#else
			// infinity maps to maxfloat
			output.bits.mantissa = 0x3ff;
			output.bits.biased_exponent = 0x1e;
#endif
		}
		else if ( (inFloat.bits.biased_exponent==0xff) && (inFloat.bits.mantissa!=0) ) 
		{ 
#if 0
			// NaN
			output.bits.mantissa = 1;
			output.bits.biased_exponent = 31;
#else
			// NaN maps to zero
			output.bits.mantissa = 0;
			output.bits.biased_exponent = 0;
#endif
		}
		else 
		{ 
			// regular number
			int new_exp = inFloat.bits.biased_exponent-127;

			if (new_exp<-24) 
			{ 
				// this maps to 0
				output.bits.mantissa = 0;
				output.bits.biased_exponent = 0;
			}

			if (new_exp<-14) 
			{
				// this maps to a denorm
				output.bits.biased_exponent = 0;
				unsigned int exp_val = ( unsigned int )( -14 - ( inFloat.bits.biased_exponent - float32bias ) );
				if( exp_val > 0 && exp_val < 11 )
				{
					output.bits.mantissa = ( 1 << ( 10 - exp_val ) ) + ( inFloat.bits.mantissa >> ( 13 + exp_val ) );
				}
			}
			else if (new_exp>15) 
			{ 
#if 0
				// map this value to infinity
				output.bits.mantissa = 0;
				output.bits.biased_exponent = 31;
#else
				// to big. . . maps to maxfloat
				output.bits.mantissa = 0x3ff;
				output.bits.biased_exponent = 0x1e;
#endif
			}
			else 
			{
				output.bits.biased_exponent = new_exp+15;
				output.bits.mantissa = (inFloat.bits.mantissa >> 13);
			}
		}
		return output.rawWord;
	}

	static float Convert16bitFloatTo32bits( unsigned short input )
	{
		float32bits output;
		float16bits inFloat;
		inFloat.rawWord = input;

		if( IsInfinity( inFloat ) )
		{
			return maxfloat16bits * ( ( inFloat.bits.sign == 1 ) ? -1.0f : 1.0f );
		}
		if( IsNaN( inFloat ) )
		{
			return 0.0;
		}
		if( inFloat.bits.biased_exponent == 0 && inFloat.bits.mantissa != 0 )
		{
			// denorm
			const float half_denorm = (1.0f/16384.0f); // 2^-14
			float mantissa = ((float)(inFloat.bits.mantissa)) / 1024.0f;
			float sgn = (inFloat.bits.sign)? -1.0f :1.0f;
			output.rawFloat = sgn*mantissa*half_denorm;
		}
		else
		{
			// regular number
			output.bits.mantissa = inFloat.bits.mantissa << (23-10);
			output.bits.biased_exponent = (inFloat.bits.biased_exponent - float16bias + float32bias) * (inFloat.bits.biased_exponent != 0);
			output.bits.sign = inFloat.bits.sign;
		}

		return output.rawFloat;
	}


	float16bits m_storage;
};
//class Vector4D					
//{
//public:
//	// Members
//	vec_t x, y, z, w;
//};
//class Vector2D					
//{
//public:
//	// Members
//	vec_t x, y;
//};
//class __declspec(align(16)) Vector4DAligned : public Vector4D
//{
//public:
//	Vector4DAligned(void) {}
//	Vector4DAligned( vec_t X, vec_t Y, vec_t Z, vec_t W );
//
//	inline void Set( vec_t X, vec_t Y, vec_t Z, vec_t W );
//
//	inline void InitZero( void );
//
//	__m128 &AsM128() { return *(__m128*)&x; }
//	const __m128 &AsM128() const { return *(const __m128*)&x; } 
//
//private:
//	// No copy constructors allowed if we're in optimal mode
//	Vector4DAligned(Vector4DAligned const& vOther);
//
//	// No assignment operators either...
//	Vector4DAligned& operator=( Vector4DAligned const& src );
//};