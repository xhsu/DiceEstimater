// DiceEstimater.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

import std.compat;

using namespace std::literals;

enum ETypes { D4, D6, D8, D10, D12, D20, DICE_TYPE_COUNTS };

namespace Statistics
{
	inline constexpr auto ChanceToKill(std::vector<double> const& rgflPercentages, int_fast16_t iHealth) noexcept
	{
		double ret{};

		for (auto&& [iDamage, flChance] : std::views::enumerate(rgflPercentages))
		{
			if (iDamage < iHealth)
				continue;

			ret += flChance;
		}

		return ret;
	}

	inline constexpr int_fast16_t Confidence(std::vector<double> const& rgflPercentages) noexcept
	{
		for (auto&& [iDamage, flChance] : std::views::enumerate(rgflPercentages))
		{
			if (flChance > 0.005)	// Greater than 0.5%
				return (int_fast16_t)iDamage;
		}

		return (int_fast16_t)-1;
	}

	inline constexpr int_fast16_t Confidence(std::vector<double> const& rgflPercentages, double flChance) noexcept
	{
		auto tmp = 1.0 - flChance;

		for (auto&& [iDamage, flChance] : std::views::enumerate(rgflPercentages))
		{
			tmp -= flChance;

			if (tmp <= 0)
				return (int_fast16_t)iDamage;
		}

		return (int_fast16_t)-1;
	}

	inline constexpr auto Range(std::vector<double> const& rgfl, int_fast16_t iMin, int_fast16_t iMax, double flStdDev) noexcept
	{
		auto tmp{ 1.0 };

		for (auto&& [iDamage, flChance] :
			std::views::enumerate(rgfl)
			| std::views::drop_while([](auto&& pr) noexcept { return std::get<1>(pr) == 0; })
			)
		{
			tmp -= 2 * flChance;	// symmetric

			if (tmp < flStdDev)
				break;

			++iMin;
			--iMax;

			if (iMin > iMax)
				break;	// ERROR!
		}

		return std::pair{ iMin, iMax };
	}
}

struct dice_t final
{
	static inline constexpr std::array DICE_MAX{ 4, 6, 8, 10, 12, 20 };
	static inline constexpr std::array DICE_NAME{ "d4"sv, "d6"sv, "d8"sv, "d10"sv, "d12"sv, "d20"sv };

	inline constexpr decltype(auto) operator[] (int idx) const noexcept { return m_dice[idx]; }
	inline constexpr decltype(auto) operator[] (int idx) noexcept { return m_dice[idx]; }

	inline constexpr auto List() const noexcept
	{
		return std::views::zip(m_dice, DICE_MAX)
			| std::views::transform([](auto&& pr) noexcept { return std::views::repeat(std::get<1>(pr), std::get<0>(pr)); })
			| std::views::join
			| std::ranges::to<std::vector>();
	}

	inline constexpr auto Possibilities() const noexcept
	{
		return std::ranges::fold_left(List(), 1, std::multiplies<int_fast16_t>{});
	}

	inline constexpr auto Range() const noexcept
	{
		int_fast16_t iMin{ m_modifier }, iMax{ m_modifier };

		//iMin += std::reduce(std::execution::par, m_dice.cbegin(), m_dice.cend());
		iMin += std::ranges::fold_left(m_dice, 0, std::plus<decltype(m_dice)::value_type>{});

		for (auto&& [cnt, MAX] : std::views::zip(m_dice, DICE_MAX))
			iMax += cnt * MAX;

		return std::pair{ iMin, iMax };
	}

	inline constexpr auto Distribution() const noexcept
	{
		std::vector<int_fast16_t> ret{};
		ret.resize(Range().second + 1);

		auto const dice = List();

		auto const fn = [&](this auto&& self, int_fast16_t val, size_t index) noexcept
			{
				if (index == dice.size())
				{
					++ret[val + m_modifier];
					return;
				}

				for (auto i = 0; i < dice[index]; ++i)
				{
					self(val + i + 1, index + 1);
				}
			};

		fn(0, 0);
		return ret;
	}

	inline constexpr auto Percentages() const noexcept
	{
		auto const possib = Possibilities();

		return
			Distribution()
			| std::views::transform([&](auto&& cnt) noexcept { return (double)cnt / (double)possib; })
			| std::ranges::to<std::vector>();
	}

	inline constexpr auto Expectation() const noexcept
	{
		auto const [iMin, iMax] = Range();
		return double(iMin + iMax) / 2.0;
	}

	inline constexpr auto ChanceToKill(int_fast16_t iHealth) const noexcept { return Statistics::ChanceToKill(Percentages(), iHealth); }

	inline constexpr auto Confidence() const noexcept { return Statistics::Confidence(Percentages()); }

	inline constexpr auto Confidence(double flChance) const noexcept { return Statistics::Confidence(Percentages(), flChance); }

	inline constexpr auto ToString() const noexcept
	{
		std::string ret{};

		for (auto&& [cnt, szName] : std::views::zip(m_dice, DICE_NAME))
		{
			if (cnt > 0)
				ret += std::format("{}{} + ", cnt, szName);
		}

		if (m_modifier)
		{
			ret += std::format("{}", m_modifier);
		}
		else
		{
			ret.pop_back();
			ret.pop_back();
			ret.pop_back();
		}

		return ret;
	}

	std::remove_cvref_t<decltype(DICE_MAX)> m_dice{};
	int_fast16_t m_modifier{};
};

inline constexpr auto operator ""_d4(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{static_cast<int_fast16_t>(iDiceCount), 0, 0, 0, 0, 0} };
}

inline constexpr auto operator ""_d6(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, static_cast<int_fast16_t>(iDiceCount), 0, 0, 0, 0} };
}

inline constexpr auto operator ""_d8(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, static_cast<int_fast16_t>(iDiceCount), 0, 0, 0} };
}

inline constexpr auto operator ""_d10(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, 0, static_cast<int_fast16_t>(iDiceCount), 0, 0} };
}

inline constexpr auto operator ""_d12(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, 0, 0, static_cast<int_fast16_t>(iDiceCount), 0} };
}

inline constexpr auto operator ""_d20(unsigned long long int iDiceCount) noexcept
{
	return dice_t{ .m_dice{0, 0, 0, 0, 0, static_cast<int_fast16_t>(iDiceCount)} };
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
		}
	};
}

inline constexpr auto operator+ (dice_t const& lhs, int_fast16_t rhs) noexcept
{
	return
		dice_t{
		.m_dice{lhs.m_dice},
		.m_modifier{lhs.m_modifier + rhs},
	};
}

inline constexpr auto operator- (dice_t const& lhs, int_fast16_t rhs) noexcept
{
	return
		dice_t{
		.m_dice{lhs.m_dice},
		.m_modifier{lhs.m_modifier - rhs},
	};
}

__forceinline constexpr auto operator+ (int_fast16_t lhs, dice_t const& rhs) noexcept { return rhs + lhs; }
__forceinline constexpr auto operator- (int_fast16_t lhs, dice_t const& rhs) noexcept { return rhs - lhs; }

void PrintDiceStat(dice_t const& dice = 2_d8 + 4_d6 + 5) noexcept
{
	std::print(u8"骰子：{}\n", dice.ToString());

	auto const possib = dice.Possibilities();
	auto const [iMin, iMax] = dice.Range();

	std::print(u8"潛在結果：{}\n", possib);
	std::print(u8"範圍：[{} - {}]\n期朢值：{}\n", iMin, iMax, dice.Expectation());
	std::print(u8"\n");

	auto const perc = dice.Percentages();

	for (auto&& [what, percentage] : std::views::enumerate(perc))
		if (percentage > 0)
			std::print(u8"{0:>2}: {1:>5.2f}% - {3:*<{2}}\n", what, percentage * 100, (int)std::round(percentage * 400), "");

	std::print(u8" - 計：{}\n", perc.size() - iMin);

	std::print(u8"\n");
	//std::print("Chance to kill HP 50: {0:.1f}%\n", dice_t::ChanceToKill(perc, 50) * 100);
	std::print(u8"存在70%之可能性使結果 >= {}\n", Statistics::Confidence(perc, 0.7));
	std::print(u8"存在80%之可能性使結果 >= {}\n", Statistics::Confidence(perc, 0.8));
	std::print(u8"存在90%之可能性使結果 >= {}\n", Statistics::Confidence(perc, 0.9));
	std::print(u8"絕對信心值 == {}\n", Statistics::Confidence(perc));

	std::print(u8"\n");

	auto const OneSigma = Statistics::Range(perc, iMin, iMax, 0.682689492137);
	auto const TwoSigma = Statistics::Range(perc, iMin, iMax, 0.954499736104);
	auto const ThreeSigma = Statistics::Range(perc, iMin, iMax, 0.997300203937);

	std::print(u8"常態分佈數據：\n");
	std::print(u8"1σ: [{} - {}]\n", OneSigma.first, OneSigma.second);
	std::print(u8"2σ: [{} - {}]\n", TwoSigma.first, TwoSigma.second);
	std::print(u8"3σ: [{} - {}]\n", ThreeSigma.first, ThreeSigma.second);

	std::print(u8"\n");
}

std::vector<std::string_view> UTIL_Split(std::string_view s, std::string_view delimiters) noexcept
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

template<typename T>
T UTIL_StrToNum(const std::string_view& sz) noexcept
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

int main(int argc, char* argv[]) noexcept
{
	using std::array;
	using std::string;
	using std::string_view;
	using std::vector;

	std::println(u8"輸入骰子及加成總和以分析。");
	std::println(u8" - 請注意：不支援減法！");
	std::println(u8" - 請注意：骰子總數超過8枚時，分析將極為緩慢。");
	std::println(u8"例如：2d8 + 4d6 + 5");

	string szInput{};
	std::getline(std::cin, szInput);

	for (auto&& c : szInput)
	{
		if (!"1234567890d+ "sv.contains(c))
		{
			std::print(u8"無效輸入：字元'{}'無法解讀\n", c);
			return 1;
		}
	}

	dice_t dice{};

	for (auto&& Word : UTIL_Split(szInput, " +"))
	{
		if (auto pos = Word.find('d'); pos != Word.npos)
		{
			auto iType = DICE_TYPE_COUNTS;

			switch (UTIL_StrToNum<int_fast16_t>(Word.substr(pos + 1)))
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
				std::print(u8"無效輸入：無效的骰子面數'{}'\n", Word);
				return 1;
			}

			// multiple dice
			if (pos != 0)
				dice[iType] += UTIL_StrToNum<int_fast16_t>(Word.substr(0, pos));
			else
				++dice[iType];
		}

		// modifier
		else
			dice.m_modifier += UTIL_StrToNum<int_fast16_t>(Word);
	}

	system("cls");
	PrintDiceStat(dice);

	system("pause");
	return 0;
}
