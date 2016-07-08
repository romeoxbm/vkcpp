#include "StringsHelper.h"
#include <cassert>
#include <algorithm>

namespace vk
{
	//--------------------------------------------------------------------------
	std::string StringsHelper::trimEnd( std::string const& input )
	{
		std::string result = input;
		result.erase( std::find_if(
						  result.rbegin(),
						  result.rend(),
						  std::not1( std::ptr_fun<int, int>( std::isspace ) )
						  ).base(), result.end()
		);
		return result;
	}
	//--------------------------------------------------------------------------
	std::string StringsHelper::toCamelCase( std::string const& value )
	{
		assert( !value.empty() && ( isupper( value[ 0 ] ) || isdigit( value[ 0 ] ) ) );
		std::string result;
		result.reserve( value.size() );
		result.push_back( value[ 0 ] );
		for( size_t i = 1; i < value.size(); i++ )
		{
			if( value[ i ] != '_' )
			{
				if( ( value[ i - 1 ] == '_' ) || isdigit( value[ i - 1 ] ) )
					result.push_back( value[ i ] );

				else
					result.push_back( tolower( value[ i ] ) );
			}
		}
		return result;
	}
	//--------------------------------------------------------------------------
	std::string StringsHelper::toUpperCase( std::string const& name )
	{
		assert( isupper( name.front() ) );
		std::string convertedName;

		for( size_t i = 0; i < name.length(); i++ )
		{
			if( isupper( name[ i ] ) &&
				( i == 0 || islower( name[ i - 1 ] ) || isdigit( name[ i - 1 ] ) ) )
				convertedName.push_back( '_' );

			convertedName.push_back( toupper( name[ i ] ) );
		}
		return convertedName;
	}
}
