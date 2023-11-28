export module Utility;

import std.compat;

export std::vector<std::string_view> UTIL_Split(std::string_view s, std::string_view delimiters) noexcept
{
	std::vector<std::string_view> ret{};

	for (auto lastPos = s.find_first_not_of(delimiters, 0), pos = s.find_first_of(delimiters, lastPos);
		s.npos != pos || s.npos != lastPos;
		lastPos = s.find_first_not_of(delimiters, pos), pos = s.find_first_of(delimiters, lastPos)
		)
	{
		ret.emplace_back(s.substr(lastPos, pos - lastPos));
	}

	return ret;
}

export template <typename T>
constexpr T UTIL_StrToNum(const std::string_view& sz) noexcept
{
	if constexpr (std::is_enum_v<T>)
	{
		if (std::underlying_type_t<T> ret{}; std::from_chars(sz.data(), sz.data() + sz.size(), ret).ec == std::errc{})
			return static_cast<T>(ret);
	}
	else
	{
		if (T ret{}; std::from_chars(sz.data(), sz.data() + sz.size(), ret).ec == std::errc{})
			return ret;
	}

	return T{};
}

export inline constexpr std::string_view UTIL_Trim(std::string_view s) noexcept
{
	constexpr std::string_view delimiters{ " \f\n\r\t\v" };
	auto const lastPos = s.find_first_not_of(delimiters, 0), pos = s.find_first_of(delimiters, lastPos);

	if (s.npos != pos || s.npos != lastPos)
		return s.substr(lastPos, pos - lastPos);

	return "";
}

static_assert(UTIL_Trim("  ") == "");
static_assert(UTIL_Trim("") == "");
static_assert(UTIL_Trim(" abc ") == "abc");
static_assert(UTIL_Trim("abc ") == "abc");
static_assert(UTIL_Trim("abc") == "abc");
static_assert(UTIL_Trim(" abc") == "abc");
