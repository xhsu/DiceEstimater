// Archieve purpose.
// Old dice type, discarded.

#if __INTELLISENSE__

#include <cstdint>

#include <algorithm>
#include <array>
#include <format>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

#else
import std.compat;
#endif

import Utility;

using namespace std::literals;

using std::array;
using std::pair;
using std::span;
using std::string;
using std::string_view;
using std::vector;

enum ETypes { D4, D6, D8, D10, D12, D20, DICE_TYPE_COUNTS };
inline constexpr std::array DICE_MAX{ (int16_t)4, (int16_t)6, (int16_t)8, (int16_t)10, (int16_t)12, (int16_t)20 };
inline constexpr std::array DICE_NAME{ "d4"sv, "d6"sv, "d8"sv, "d10"sv, "d12"sv, "d20"sv };

struct dice_t final
{
	inline constexpr decltype(auto) operator[] (this auto&& self, int idx) noexcept { return self.m_dice[idx]; }

	inline constexpr auto List() const noexcept
	{
		/* #UPDATE_AT_CPP23_P2647R1 */ constexpr auto fnRepeat =
			[](auto&& pr) /* #UPDATE_AT_CPP23_static_operator */ noexcept
			{
				auto&& [iCount, iMax] = pr;

				if (iCount < 0)
					return std::views::repeat(static_cast<int16_t>(-iMax), static_cast<int16_t>(-iCount));

				return std::views::repeat(iMax, iCount);
			};

		return std::views::zip(m_dice, DICE_MAX)
			| std::views::transform(fnRepeat)
			| std::views::join
			| std::ranges::to<vector>();
	}

	inline constexpr auto Possibilities() const noexcept
	{
		return std::ranges::fold_left(List() | std::views::transform([](auto&& n) noexcept { return n < 0 ? -n : n; }), 1, std::multiplies<>{});
	}

	inline constexpr auto LowerBound() const noexcept
	{
		//return std::ranges::fold_left(m_dice, m_modifier, std::plus<>{});

		int16_t ret{ m_modifier };
		for (auto&& [count, maximum] : std::views::zip(m_dice, DICE_MAX))
			ret += count < 0 ? (count * maximum) : count;

		return ret;
	}

	inline constexpr auto UpperBound() const noexcept
	{
		//return std::ranges::fold_left(std::views::zip_transform(std::multiplies<>{}, m_dice, DICE_MAX), m_modifier, std::plus<>{});

		int16_t ret{ m_modifier };
		for (auto&& [count, maximum] : std::views::zip(m_dice, DICE_MAX))
			ret += count < 0 ? count : (count * maximum);

		return ret;
	}

	inline constexpr auto Range() const noexcept
	{
		return pair{
			LowerBound(),
			UpperBound(),
		};
	}

	inline constexpr auto Distribution() const noexcept
	{
		auto const [lower_bound, upper_bound] = Range();
		auto const dice = List();
		auto const offset = m_modifier - lower_bound;
		auto const should_reserve = upper_bound - lower_bound + 1;

		vector<uint32_t> ret{};	// flat map? #UPDATE_AT_CPP23_flat_meow
		ret.resize(should_reserve);

		auto const fnRecursive = [&](this auto&& self, int16_t val, size_t index) noexcept
			{
				if (index == dice.size())
				{
					++ret[val + offset];
					return;
				}

				auto const start = dice[index] < 0 ? dice[index] : 1;
				auto const stop = dice[index] < 0 ? -1 : dice[index];

				for (auto i = start; i <= stop; ++i)
				{
					self(val + i, index + 1);
				}
			};

		fnRecursive(0, 0);
		return ret;
	}

	inline constexpr auto Percentages() const noexcept
	{
		auto const possib = (double)Possibilities();

		return
			Distribution()
			| std::views::transform([&](auto&& cnt) noexcept { return (double)cnt / possib; })
			| std::ranges::to<vector>();
	}

	inline constexpr auto Expectation() const noexcept
	{
		auto const [iMin, iMax] = Range();
		return double(iMin + iMax) / 2.0;
	}

	// due to the inline nature of constexpr function, this would cause linker error, as all the referenced function are constepxr.

	//inline constexpr auto Confidence() const noexcept { return Statistics::Confidence(LowerBound(), Percentages()); }

	//inline constexpr auto Confidence(double flChance) const noexcept { return Statistics::Confidence(LowerBound(), Percentages(), flChance); }

	inline constexpr auto ToString() const noexcept
	{
		string ret{};

		for (auto&& [cnt, szName] : std::views::zip(m_dice, DICE_NAME))
		{
			if (cnt != 0)
				ret += std::format("{:+}{}", cnt, szName);
		}

		if (m_modifier)
		{
			ret += std::format("{:+}", m_modifier);
		}

		if (!ret.empty() && ret.front() == '+')
			ret.erase(ret.begin());

		for (auto it = ret.begin(); it != ret.end(); ++it)
		{
			if ("^*/%+-"sv.contains(*it))
			{
				it = ret.insert(it, ' ');
				it += 2;
				it = ret.insert(it, ' ');
				// now iter points to the inserted space, not the operator.
			}
		}

		return ret;
	}

	std::remove_cvref_t<decltype(DICE_MAX)> m_dice{};
	int16_t m_modifier{};
};

inline constexpr auto operator ""_d4(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{static_cast<int16_t>(iDiceCount), 0, 0, 0, 0, 0} };
}

inline constexpr auto operator ""_d6(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, static_cast<int16_t>(iDiceCount), 0, 0, 0, 0} };
}

inline constexpr auto operator ""_d8(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, static_cast<int16_t>(iDiceCount), 0, 0, 0} };
}

inline constexpr auto operator ""_d10(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, 0, static_cast<int16_t>(iDiceCount), 0, 0} };
}

inline constexpr auto operator ""_d12(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, 0, 0, static_cast<int16_t>(iDiceCount), 0} };
}

inline constexpr auto operator ""_d20(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, 0, 0, 0, static_cast<int16_t>(iDiceCount)} };
}

inline constexpr auto operator+ (dice_t const& lhs, dice_t const& rhs) noexcept
{
	return
		dice_t{
			.m_dice{
				lhs[D4] + rhs[D4],
				lhs[D6] + rhs[D6],
				lhs[D8] + rhs[D8],
				lhs[D10] + rhs[D10],
				lhs[D12] + rhs[D12],
				lhs[D20] + rhs[D20],
			},
			.m_modifier{ lhs.m_modifier + rhs.m_modifier },
	};
}

inline constexpr auto operator- (dice_t const& lhs, dice_t const& rhs) noexcept
{
	return
		dice_t{
			.m_dice{
				lhs[D4] - rhs[D4],
				lhs[D6] - rhs[D6],
				lhs[D8] - rhs[D8],
				lhs[D10] - rhs[D10],
				lhs[D12] - rhs[D12],
				lhs[D20] - rhs[D20],
			},
			.m_modifier{ lhs.m_modifier - rhs.m_modifier },
	};
}

inline constexpr auto operator+ (dice_t const& lhs, int16_t rhs) noexcept
{
	return
		dice_t{
		.m_dice{ lhs.m_dice },
		.m_modifier{ lhs.m_modifier + rhs },
	};
}

inline constexpr auto operator- (dice_t const& lhs, int16_t rhs) noexcept
{
	return
		dice_t{
		.m_dice{ lhs.m_dice },
		.m_modifier{ lhs.m_modifier - rhs },
	};
}

__forceinline constexpr auto operator+ (int16_t lhs, dice_t const& rhs) noexcept { return rhs + lhs; }
__forceinline constexpr auto operator- (int16_t lhs, dice_t const& rhs) noexcept { return rhs - lhs; }

extern vector<string_view> ShuntingYardAlgorithm(string_view s);

namespace Dice
{
	/*constexpr*/ dice_t CreateObject(string const& szInput) noexcept
	{
		vector<dice_t> dice_stack{};

		for (auto&& token : ShuntingYardAlgorithm(szInput))
		{
			// non-token?
			if (token.length() > 1 || !"+-"sv.contains(token[0]))
			{
				dice_t dice{};

				// Is die?
				if (auto const pos = token.find('d'); pos != token.npos)
				{
					auto iType = DICE_TYPE_COUNTS;

					switch (UTIL_StrToNum<int16_t>(token.substr(pos + 1)))
					{
					case 4:
						iType = D4;
						break;
					case 6:
						iType = D6;
						break;
					case 8:
						iType = D8;
						break;
					case 10:
						iType = D10;
						break;
					case 12:
						iType = D12;
						break;
					case 20:
						iType = D20;
						break;

					default:
						std::print(u8"無效輸入：無效的骰子面數'{}'\n", token);
						return {};
					}

					// multiple dice
					if (pos != 0)
						dice[iType] += UTIL_StrToNum<int16_t>(token.substr(0, pos));

					// only one.
					else
						++dice[iType];
				}

				// Or modifier?
				else
					dice.m_modifier += UTIL_StrToNum<int16_t>(token);

				// save result
				dice_stack.emplace_back(std::move(dice));
			}
			else if ("+-"sv.contains(token[0]))
			{
				auto const arg_count = 2;
				auto const first_param_pos = dice_stack.size() - arg_count;
				auto const params = span{ dice_stack.data() + first_param_pos, arg_count };

				dice_t res{};
				switch (token[0])
				{
				case '+':
					res = params[0] + params[1];
					break;
				case '-':
					res = params[0] - params[1];
					break;

				default:
					std::print(u8"無效輸入：不支援的運算子'{}'\n", token);
					return {};
				}

				dice_stack.erase(dice_stack.begin() + first_param_pos, dice_stack.end());
				dice_stack.emplace_back(std::move(res));
			}
			else
				std::unreachable();
		}

		return dice_stack.front();
	}
}
