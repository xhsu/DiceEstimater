// DiceEstimater.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

import std.compat;

import Utility;

using std::array;
using std::pair;
using std::span;
using std::string;
using std::string_view;
using std::tuple;
using std::vector;

using namespace std::literals;

enum ETypes { D4, D6, D8, D10, D12, D20, DICE_TYPE_COUNTS };

namespace Statistics
{
	constexpr auto ChanceToKill(int32_t minimum, vector<double> const& rgflPercentages, int16_t iHealth) noexcept
	{
		double ret{};

		for (auto&& [iDamage, flChance] : std::views::zip(std::views::iota(minimum), rgflPercentages))
		{
			if (iDamage < iHealth)
				continue;

			ret += flChance;
		}

		return ret;
	}

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

	constexpr auto Range(vector<double> const& rgfl, int16_t iMin, int16_t iMax, double flStdDev) noexcept
	{
		auto tmp{ 1.0 };

		for (auto&& [iDamage, flChance] : std::views::zip(std::views::iota(iMin), rgfl))
		{
			tmp -= 2 * flChance;	// symmetric

			if (tmp < flStdDev)
				break;

			++iMin;
			--iMax;

			if (iMin > iMax)
				break;	// ERROR!
		}

		return pair{ iMin, iMax };
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

struct dice_t final
{
	static inline constexpr std::array DICE_MAX{ (int16_t)4, (int16_t)6, (int16_t)8, (int16_t)10, (int16_t)12, (int16_t)20 };
	static inline constexpr std::array DICE_NAME{ "d4"sv, "d6"sv, "d8"sv, "d10"sv, "d12"sv, "d20"sv };

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

	inline constexpr auto Confidence() const noexcept { return Statistics::Confidence(LowerBound(), Percentages()); }

	inline constexpr auto Confidence(double flChance) const noexcept { return Statistics::Confidence(LowerBound(), Percentages(), flChance); }

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

void PrintDiceStat(dice_t const& dice = 2_d8 + 4_d6 + 5) noexcept
{
	std::print(u8"骰子：{}\n", dice.ToString());

	auto const possib = dice.Possibilities();
	auto const [iMin, iMax] = dice.Range();
	auto const offset = dice.m_modifier - iMin;

	std::print(u8"潛在結果：{}\n", possib);
	std::print(u8"範圍：[{} - {}]\n期朢值：{}\n", iMin, iMax, dice.Expectation());
	std::print(u8"\n");

	auto const perc = dice.Percentages();
	auto const peak = std::ranges::max(perc);	// for normalizing graph

	for (auto&& [damage, percentage] : std::views::zip(std::views::iota(iMin), perc))
		std::print(u8"{0:>2}: {1:>5.2f}% - {3:*<{2}}\n", damage, percentage * 100, (int)std::round(percentage / peak * 100), "");

	std::print(u8" - 計：{}\n", perc.size());

	std::print(u8"\n");
	std::print(u8"存在70%之可能性使結果 >= {}\n", Statistics::Confidence(iMin, perc, 0.7));
	std::print(u8"存在80%之可能性使結果 >= {}\n", Statistics::Confidence(iMin, perc, 0.8));
	std::print(u8"存在90%之可能性使結果 >= {}\n", Statistics::Confidence(iMin, perc, 0.9));
	std::print(u8"絕對信心值 == {}\n", Statistics::Confidence(iMin, perc));

	std::print(u8"\n");

	auto const OneSigma = Statistics::Range(perc, iMin, iMax, 0.682689492137);
	auto const TwoSigma = Statistics::Range(perc, iMin, iMax, 0.954499736104);
	auto const ThreeSigma = Statistics::Range(perc, iMin, iMax, 0.997300203937);

	std::print(u8"高斯分佈數據：\n");
	std::print(u8"1σ: [{} - {}]\n", OneSigma.first, OneSigma.second);
	std::print(u8"2σ: [{} - {}]\n", TwoSigma.first, TwoSigma.second);
	std::print(u8"3σ: [{} - {}]\n", ThreeSigma.first, ThreeSigma.second);

	std::print(u8"\n");

	if (dice[D20] == 1)
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

			auto const [pass, pass_when_adv, pass_when_disadv] = Statistics::Challenge(iMin, perc, i);

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

extern vector<string_view> ShuntingYardAlgorithm(string_view s);

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
		std::println(u8"例如：2d8 + 4d6 + 5");

		std::getline(std::cin, szInput);
	}

	for (auto&& c : szInput)
	{
		if (!"1234567890d+- "sv.contains(c))
		{
			std::print(u8"無效輸入：字元'{}'無法解讀\n", c);
			return 1;
		}
	}

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
					return 1;
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
				return 1;
			}

			dice_stack.erase(dice_stack.begin() + first_param_pos, dice_stack.end());
			dice_stack.emplace_back(std::move(res));
		}
		else
			std::unreachable();
	}

	{
		//auto_timer_t t{};
		system("cls");
		PrintDiceStat(dice_stack.front());
	}

	system("pause");
	return 0;
}
