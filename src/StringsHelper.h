#ifndef STRINGSHELPER_H
#define STRINGSHELPER_H

#include <string>

namespace vk
{
	class StringsHelper
	{
	public:
		static std::string trimEnd( std::string const& input );

		static std::string toCamelCase( std::string const& value );

		static std::string toUpperCase( std::string const& name );

		static std::string strip( std::string const& value,
								  std::string const& prefix,
								  std::string const& tag = "" );

	private:
		StringsHelper() {}
		~StringsHelper() {}
	};
}
#endif // STRINGSHELPER_H
