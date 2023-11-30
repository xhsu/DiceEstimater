// DiceEstimater.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#if __INTELLISENSE__
#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <print>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>
#else
import std.compat;
#endif

import Utility;

using std::array;
using std::pair;
using std::string;
using std::string_view;
using std::tuple;
using std::vector;

using namespace std::literals;

namespace Statistics
{
	constexpr int16_t Confidence(int32_t minimum, vector<double> const& rgflPercentages) noexcept
	{
		for (auto&& [iDamage, flChance] : std::views::zip(std::views::iota(minimum), rgflPercentages))
		{
			if (flChance > 0.005)	// Greater than 0.5%
				return (int16_t)iDamage;
		}

		return (int16_t)-1;
	}

	constexpr int16_t Confidence(int32_t minimum, vector<double> const& rgflPercentages, double flChance) noexcept
	{
		auto tmp = 1.0 - flChance;

		for (auto&& [iDamage, flChance] : std::views::zip(std::views::iota(minimum), rgflPercentages))
		{
			tmp -= flChance;

			if (tmp <= 0)
				return (int16_t)iDamage;
		}

		return (int16_t)-1;
	}

	constexpr auto IntervalEstimate(vector<double> const& rgflPercentages, int16_t iLeftBound, int16_t iRightBound, double flStdDev) noexcept
	{
		auto tmp{ 1.0 };

		for (auto&& [iDamage, flChance] : std::views::zip(std::views::iota(iLeftBound), rgflPercentages))
		{
			tmp -= 2 * flChance;	// symmetric

			if (tmp < flStdDev)
				break;

			++iLeftBound;
			--iRightBound;

			if (iLeftBound > iRightBound)
				break;	// ERROR!
		}

		return pair{ iLeftBound, iRightBound };
	}

	constexpr auto Challenge(int32_t minimum, vector<double> const& rgflPercentages, int16_t dc) noexcept
	{
		double pass{};

		for (auto&& [iResult, flChance] : std::views::zip(std::views::iota(minimum), rgflPercentages))
		{
			if (iResult < dc)
				continue;

			pass += flChance;
		}

		auto const fail = 1.0 - pass;
		auto const pass_when_adv = 1.0 - fail * fail;
		auto const pass_when_disadv = pass * pass;

		return tuple{
			pass,
			pass_when_adv,
			pass_when_disadv,
		};
	}

	constexpr auto Possibilities(vector<int16_t> const& dice) noexcept
	{
		return std::ranges::fold_left(
			dice | std::views::transform([](int16_t n) noexcept -> uint64_t { return n < 0 ? -n : n; }),	// promote!
			1,
			std::multiplies<>{}
		);
	}

	constexpr auto LowerBound(int16_t modifier, vector<int16_t> const& dice) noexcept
	{
		auto const fn =
			[](int16_t n) noexcept -> int16_t
			{
				// lowest: substraction die takes its maximum and others take minimum.
				return n < 0 ? n : 1;
			};

		// force the return type to be int16_t rather than promoted int32_t.
		return std::ranges::fold_left(dice | std::views::transform(fn), modifier, std::plus<int16_t>{});
	}

	constexpr auto UpperBound(int16_t modifier, vector<int16_t> const& dice) noexcept
	{
		auto const fn =
			[](int16_t n) noexcept -> int16_t
			{
				// highest: substraction die takes its minimum and others take maximum.
				return n < 0 ? -1 : n;
			};

		// force the return type to be int16_t rather than promoted int32_t.
		return std::ranges::fold_left(dice | std::views::transform(fn), modifier, std::plus<int16_t>{});
	}

	constexpr auto Range(int16_t modifier, vector<int16_t> const& dice) noexcept { return pair{ LowerBound(modifier, dice), UpperBound(modifier, dice) }; }

	constexpr auto Distribution(int16_t modifier, int16_t lower_bound, int16_t upper_bound, vector<int16_t> const& dice) noexcept
	{
		auto const offset = modifier - lower_bound;
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

	constexpr auto Percentages(int16_t modifier, vector<int16_t> const& dice) noexcept
	{
		auto const total = static_cast<double>(Possibilities(dice));

		return
			Distribution(modifier, LowerBound(modifier, dice), UpperBound(modifier, dice), dice)
			| std::views::transform([&](auto&& cnt) noexcept { return (double)cnt / total; })
			| std::ranges::to<vector>();
	}

	constexpr auto Expectation(int16_t modifier, vector<int16_t> const& dice) noexcept
	{
		auto const E =
			[](auto&& x) noexcept -> double
			{
				// retain the lhs, but take E(x) on the number.

				if (x < 0)
					return ((double)x - 1.0) / 2.0;

				return (1.0 + (double)x) / 2.0;
			};

		return std::ranges::fold_left(dice | std::views::transform(E), modifier, std::plus<>{});
	}

#define TEST_DICE vector<int16_t>{ -4, 10, 10, 10 }
	static_assert(
		Possibilities(TEST_DICE) == 4000
		and LowerBound(4, TEST_DICE) == 3 and UpperBound(4, TEST_DICE) == 33
		and Confidence(/* lower_bound */3, Percentages(4, TEST_DICE)) == 7
		and Expectation(4, TEST_DICE) == 18
		);
#undef TEST_DICE
}

namespace Dice
{
	struct Arrange final
	{
		/*#UPDATE_AT_CPP23_static_operator*/ constexpr bool operator() (int16_t lhs, int16_t rhs) const noexcept
		{
			/*
			purpose:
				replacement of std::less when dice involved.
				positive dice always goes first, and negative dice follows.
				e.g.
					d4 d4 d6 d10 -d4 -d8
			*/

			auto const bLhsPositive = lhs > 0;
			auto const bRhsPositive = rhs > 0;

			if (bLhsPositive && bRhsPositive)
				return lhs < rhs;	// d4 < d6

			if (bLhsPositive && !bRhsPositive)
				return true;	// d4 < -d4

			if (!bLhsPositive && bRhsPositive)
				return false;	// -d4 > d4

			if (!bLhsPositive && !bRhsPositive)
				return lhs > rhs;	// -d4 < -d6, but methmatically -4 is greater than -6, so...

			std::unreachable();
		}
	};

	constexpr void Sort(vector<int16_t>* prgiDice) noexcept { std::ranges::sort(*prgiDice, Arrange{}); }

	constexpr auto Count(vector<int16_t> const& dice, int16_t face) noexcept { return std::ranges::count(dice, face); }

	string ToString(int16_t modifier, vector<int16_t> const& dice) noexcept
	{
		static constexpr std::array POS_DIE_TYPES{ 4, 6, 8, 10, 12, 20, };
		static constexpr std::array NEG_DIE_TYPES{ -4, -6, -8, -10, -12, -20, };	// #UPDATE_AT_CPP26_static_in_constexpr_fn

		string ret{};

		// normal dice first, then debuffs.
		// hide '1' from 1d4.

		for (auto&& type : POS_DIE_TYPES)
			if (auto const count = Count(dice, type); count > 0)
				ret += std::format("{}d{}", count == 1 ? "+"s : std::format("{:+}", count), type);

		for (auto&& type : NEG_DIE_TYPES)
			if (auto const count = Count(dice, type); count > 0)
				ret += std::format("{}d{}", count == 1 ? "-"s : std::format("{}", -count), -type);

		if (modifier)
			ret += std::format("{:+}", modifier);

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
}

struct auto_timer_t final
{
	auto_timer_t() noexcept
		: m_start{ std::chrono::high_resolution_clock::now() }
	{
	}

	auto_timer_t(auto&&...) noexcept = delete;
	auto_timer_t& operator= (auto&&...) noexcept = delete;
	~auto_timer_t() noexcept
	{
		auto const end_time = std::chrono::high_resolution_clock::now();
		auto const delta = end_time - m_start;

		std::println(u8"╔══════════════════════╗");
		std::println(u8"║ Time passed: {:.4f}s ║", std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() / (double)1e9);
		std::println(u8"╚══════════════════════╝");
	}

	decltype(std::chrono::high_resolution_clock::now()) m_start{};
};

void PrintDiceStat(int16_t modifier, vector<int16_t> const& dice) noexcept
{
	std::print(u8"骰子：{}\n", Dice::ToString(modifier, dice));

	auto const possibilities = Statistics::Possibilities(dice);
	auto const [iMin, iMax] = Statistics::Range(modifier, dice);
	auto const offset = modifier - iMin;

	std::print(u8"潛在結果：{}\n", possibilities);
	std::print(u8"範圍：[{} - {}]\n期朢值：{}\n", iMin, iMax, Statistics::Expectation(modifier, dice));
	std::print(u8"\n");

	auto const percentages = Statistics::Percentages(modifier, dice);
	auto const peak = std::ranges::max(percentages);	// for normalizing graph

	for (auto&& [damage, percentage] : std::views::zip(std::views::iota(iMin), percentages))
		std::print(u8"{0:>2}: {1:>5.2f}% - {3:*<{2}}\n", damage, percentage * 100, (int)std::round(percentage / peak * 100), "");

	std::print(u8" - 計：{}\n", percentages.size());

	std::print(u8"\n");
	std::print(u8"存在70%之可能性使結果 >= {}\n", Statistics::Confidence(iMin, percentages, 0.7));
	std::print(u8"存在80%之可能性使結果 >= {}\n", Statistics::Confidence(iMin, percentages, 0.8));
	std::print(u8"存在90%之可能性使結果 >= {}\n", Statistics::Confidence(iMin, percentages, 0.9));
	std::print(u8"絕對信心值 == {}\n", Statistics::Confidence(iMin, percentages));

	std::print(u8"\n");

	auto const OneSigma = Statistics::IntervalEstimate(percentages, iMin, iMax, 0.682689492137);
	auto const TwoSigma = Statistics::IntervalEstimate(percentages, iMin, iMax, 0.954499736104);
	auto const ThreeSigma = Statistics::IntervalEstimate(percentages, iMin, iMax, 0.997300203937);

	std::print(u8"高斯分佈數據：\n");
	std::print(u8"1σ: [{} - {}]\n", OneSigma.first, OneSigma.second);
	std::print(u8"2σ: [{} - {}]\n", TwoSigma.first, TwoSigma.second);
	std::print(u8"3σ: [{} - {}]\n", ThreeSigma.first, ThreeSigma.second);

	std::print(u8"\n");

	// extra info for skill test mode.
	if (Dice::Count(dice, 20) == 1)
	{
		auto const TwoCharactersWide = std::formatted_size(u8" {} ", u8"二字") + 2;
		auto const ThreeCharactersWide = std::formatted_size(u8" {} ", u8"三個字") + 1 + 2;

		std::print(u8"╔{0:═^{1}}╤{0:═^{2}}╤{0:═^{1}}╤{0:═^{1}}╗\n", "", TwoCharactersWide, ThreeCharactersWide);
		std::print(u8"║{0:^{4}}│{1:^{5}}│{2:^{4}}│{3:^{4}}║\n", u8"難度", u8"成功率", u8"優勢", u8"劣勢", TwoCharactersWide, ThreeCharactersWide);
		std::print(u8"╟{0:─^{1}}┼{0:─^{2}}┼{0:─^{1}}┼{0:─^{1}}╢\n", "", TwoCharactersWide, ThreeCharactersWide);

		static constexpr array rgiChallenges{ 2, 5, -1, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, -1, 25, 30, 35 };

		for (auto i : rgiChallenges)
		{
			if (i < 1)
			{
				std::print(u8"╟{0:─^{1}}┼{0:─^{2}}┼{0:─^{1}}┼{0:─^{1}}╢\n", "", TwoCharactersWide, ThreeCharactersWide);
				continue;
			}

			auto const [pass, pass_when_adv, pass_when_disadv] = Statistics::Challenge(iMin, percentages, i);

			std::print(
				u8"║{0:^{4}}│{1:^{5}}│{2:^{4}}│{3:^{4}}║\n",
				i,
				std::format("{:.2f}%", pass * 100.0),
				std::format("{:.2f}%", pass_when_adv * 100.0),
				std::format("{:.2f}%", pass_when_disadv * 100.0), TwoCharactersWide, ThreeCharactersWide
			);
		}

		std::print(u8"╚{0:═^{1}}╧{0:═^{2}}╧{0:═^{1}}╧{0:═^{1}}╝\n", "", TwoCharactersWide, ThreeCharactersWide);
		std::print(u8"\n");
	}
}

int main(int argc, char* argv[]) noexcept
{
	string szInput{};

	if (argc > 1)
	{
		for (auto i = 1; i < argc; ++i)
			szInput += argv[i];
	}
	else
	{
		std::println(u8"輸入骰子及加成總和以分析。");
		std::println(u8" - 請注意：骰子總數超過8枚時，分析將極為緩慢。");
		std::println(u8"例如：2d8 + 4d6 + 5\n　　　d20 + d4 + 3 - 1");	// full width space in use. '　', U+3000

		std::getline(std::cin, szInput);
	}

	for (auto&& c : szInput)
	{
		if (c == 'D')
			c = 'd';

		if (!"1234567890d+- "sv.contains(c))
		{
			std::print(u8"無效輸入：字元'{}'無法解讀\n", c);
			return 1;
		}
	}

	static constexpr string_view delimiters = "+-"sv;
	string_view const input_view{ szInput };	// make sure sz.substr() is returning string_view.
	vector<string_view> identifiers{};
	bool phase = false;

	for (size_t pos = input_view.find_first_of(delimiters), last_pos = 0;
		pos != input_view.npos || last_pos != input_view.npos;
		phase = !phase, last_pos = pos, pos = (phase ? input_view.find_first_not_of(delimiters, pos) : input_view.find_first_of(delimiters, pos))
		)
	{
		if (auto const trimmed = UTIL_Trim(input_view.substr(last_pos, pos - last_pos)); trimmed.length())
		{
			if (phase)	// split operators.
			{
				for (auto& c : trimmed)
					identifiers.emplace_back(&c, 1);
			}
			else
				identifiers.emplace_back(trimmed);
		}
	}

	vector<int16_t> dice{};
	int16_t modifier = 0;
	bool negative = false;
	phase = false;

	for (auto&& token : identifiers)
	{
		// non-token?
		if (token.length() > 1 || !delimiters.contains(token[0]))
		{
			// enforce the syntax.
			if (phase)
			{
				std::print(u8"格式錯誤：「運算子」應與「骰子」交替。\n\t錯誤位於'{}'處。\n", token);
				return 1;
			}

			// Is die?
			if (auto const pos = token.find('d'); pos != token.npos)
			{
				auto const face = UTIL_StrToNum<int16_t>(token.substr(pos + 1)) * (int16_t)(negative ? -1 : 1);

				// multiple dice
				if (pos != 0)
				{
					dice.append_range(
						std::views::repeat(
							face,
							UTIL_StrToNum<int16_t>(token.substr(0, pos))	// how many?
						)
					);
				}

				// only one.
				else
					dice.push_back(face);
			}

			// Or modifier?
			else
				modifier += UTIL_StrToNum<int16_t>(token) * (negative ? -1 : 1);
		}

		// operators?
		else if (delimiters.contains(token[0]))
		{
			// enforce the syntax.
			if (!phase)
			{
				std::print(u8"格式錯誤：「運算子」應與「骰子」交替。\n\t錯誤位於'{}'處。\n", token);
				return 1;
			}

			negative = token[0] == '-';
		}

		else
		{
			std::print(u8"無效輸入：不支援的運算子'{}'\n", token);
			return 1;
		}

		phase = !phase;
	}

	Dice::Sort(&dice);

	// just timing it for fun.
	{
		//auto_timer_t t{};
		system("cls");
		PrintDiceStat(modifier, dice);
	}

	system("pause");
	return 0;
}
