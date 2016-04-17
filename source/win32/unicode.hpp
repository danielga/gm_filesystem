#include <string>
#include <iterator>

namespace Unicode
{

namespace UTF8
{

template<typename Input>
Input Decode( Input begin, Input end, uint32_t &output, uint32_t replace = 0 )
{
	static const uint32_t offsets[] = {
		0x00000000,
		0x00003080,
		0x000E2080,
		0x03C82080
	};

	uint8_t ch = static_cast<uint8_t>( *begin );
	int32_t trailingBytes = -1;
	if( ch < 128 )
		trailingBytes = 0;
	else if( ch < 192 )
		/* do nothing, invalid byte */;
	else if( ch < 224 )
		trailingBytes = 1;
	else if( ch < 240 )
		trailingBytes = 2;
	else if( ch < 248 )
		trailingBytes = 3;
	else
		/* do nothing, invalid byte, used for 5 and 6 bytes UTF8 sequences */;

	if( trailingBytes == -1 )
	{
		++begin;
		output = replace;
	}
	else if( begin + trailingBytes < end )
	{
		output = 0;
		switch( trailingBytes )
		{
		case 3:
			output += static_cast<uint8_t>( *begin );
			++begin;
			output <<= 6;

		case 2:
			output += static_cast<uint8_t>( *begin );
			++begin;
			output <<= 6;

		case 1:
			output += static_cast<uint8_t>( *begin );
			++begin;
			output <<= 6;

		case 0:
			output += static_cast<uint8_t>( *begin );
			++begin;
		}

		output -= offsets[trailingBytes];
	}
	else
	{
		begin = end;
		output = replace;
	}

	return begin;
}

template<typename Output>
Output Encode( uint32_t input, Output output, uint8_t replace = 0 )
{
	static const uint8_t firstBytes[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

	if( input > 0x0010FFFF || ( input >= 0xD800 && input <= 0xDBFF ) )
	{
		if( replace )
		{
			*output = replace;
			++output;
		}
	}
	else
	{
		size_t bytestoWrite = 1;
		if( input < 0x80 )
			bytestoWrite = 1;
		else if( input < 0x800 )
			bytestoWrite = 2;
		else if( input < 0x10000 )
			bytestoWrite = 3;
		else if( input <= 0x0010FFFF )
			bytestoWrite = 4;

		uint8_t bytes[4];
		switch( bytestoWrite )
		{
		case 4:
			bytes[3] = static_cast<uint8_t>( ( input | 0x80 ) & 0xBF );
			input >>= 6;

		case 3:
			bytes[2] = static_cast<uint8_t>( ( input | 0x80 ) & 0xBF );
			input >>= 6;

		case 2:
			bytes[1] = static_cast<uint8_t>( ( input | 0x80 ) & 0xBF );
			input >>= 6;

		case 1:
			bytes[0] = static_cast<uint8_t>( input | firstBytes[bytestoWrite] );
		}

		output = std::copy( bytes, bytes + bytestoWrite, output );
	}

	return output;
}

template<typename Input>
std::wstring ToUTF16( Input begin, Input end )
{
	std::wstring out;
	out.reserve( end - begin );
	auto inserter = std::back_inserter( out );

	uint32_t codepoint = 0;
	while( begin != end )
	{
		begin = Decode( begin, end, codepoint );
		inserter = UTF16::Encode( codepoint, inserter );
	}

	return out;
}

}

namespace UTF16
{

template<typename Output>
Output Encode( uint32_t input, Output output, uint16_t replace = 0 )
{
	if( input < 0xFFFF )
	{
		if( input >= 0xD800 && input <= 0xDFFF )
		{
			if( replace )
			{
				*output = replace;
				++output;
			}
		}
		else
		{
			*output = static_cast<uint16_t>( input );
			++output;
		}
	}
	else if( input > 0x0010FFFF )
	{
		if( replace )
		{
			*output = replace;
			++output;
		}
	}
	else
	{
		input -= 0x0010000;
		*output = static_cast<uint16_t>( ( input >> 10 ) + 0xD800 );
		++output;
		*output = static_cast<uint16_t>( ( input & 0x3FFUL ) + 0xDC00 );
		++output;
	}

	return output;
}

template<typename Input>
Input Decode( Input begin, Input end, uint32_t &output, uint32_t replace = 0 )
{
	uint16_t first = *begin;
	++begin;

	if( first >= 0xD800 && first <= 0xDBFF )
	{
		if( begin < end )
		{
			uint32_t second = *begin;
			++begin;
			if( second >= 0xDC00 && second <= 0xDFFF )
				output = ( ( first - 0xD800 ) << 10 ) + second - 0xDC00 + 0x00010000;
			else
				output = replace;
		}
		else
		{
			begin = end;
			output = replace;
		}
	}
	else
	{
		output = first;
	}

	return begin;
}

template<typename Input>
std::string ToUTF8( Input begin, Input end )
{
	std::string out;
	out.reserve( end - begin );
	auto inserter = std::back_inserter( out );

	uint32_t codepoint = 0;
	while( begin != end )
	{
		begin = Decode( begin, end, codepoint );
		inserter = UTF8::Encode( codepoint, inserter );
	}

	return out;
}

}

}
