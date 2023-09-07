// DiceEstimater.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

import std.compat;

struct dice_t final
{
	enum { D4, D6, D8, D10, D12, D20, DICE_TYPE_COUNTS };
	static inline constexpr std::array<int_fast16_t, DICE_TYPE_COUNTS> DICE_MAX{ 4, 6, 8, 10, 12, 20 };

	inline constexpr decltype(auto) operator[] (int idx) const noexcept { return m_dice[idx]; }
	inline constexpr decltype(auto) operator[] (int idx) noexcept { return m_dice[idx]; }

	inline constexpr auto operator+ (dice_t const& rhs) const noexcept
	{
		return
			dice_t{
			.m_dice{
				m_dice[D4] + rhs[D4],
				m_dice[D6] + rhs[D6],
				m_dice[D8] + rhs[D8],
				m_dice[D10] + rhs[D10],
				m_dice[D12] + rhs[D12],
				m_dice[D20] + rhs[D20],
			}
		};
	}

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
		int_fast16_t iMin{}, iMax{};

		//iMin = std::reduce(std::execution::par, m_dice.cbegin(), m_dice.cend());
		iMin = std::ranges::fold_left(m_dice, 0, std::plus<decltype(m_dice)::value_type>{});

		for (auto&& [cnt, MAX] : std::views::zip(m_dice, DICE_MAX))
			iMax += cnt * MAX;

		return std::pair{ iMin, iMax };
	}

	inline constexpr auto Distribution() const noexcept
	{
		std::vector<int_fast16_t> ret{};
		ret.resize(Range().second + 1);

		auto const dice = List();

		auto const fn = [&](this auto&& self, uint32_t val, size_t index) noexcept
			{
				if (index == dice.size())
				{
					++ret[val];
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

	inline constexpr static auto ChanceToKill(std::vector<double> const& rgflPercentages, int_fast16_t iHealth) noexcept
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

	inline constexpr auto ChanceToKill(int_fast16_t iHealth) const noexcept { return ChanceToKill(Percentages(), iHealth); }

	inline constexpr static int_fast16_t Confidence(std::vector<double> const& rgflPercentages) noexcept
	{
		for (auto&& [iDamage, flChance] : std::views::enumerate(rgflPercentages))
		{
			if (flChance > 0.005)	// Greater than 0.5%
				return (int_fast16_t)iDamage;
		}

		return (int_fast16_t)-1;
	}

	inline constexpr auto Confidence() const noexcept { return Confidence(Percentages()); }

	inline constexpr static int_fast16_t Confidence(std::vector<double> const& rgflPercentages, double flChance) noexcept
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

	decltype(DICE_MAX) m_dice{};
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

int main(int argc, char* argv[]) noexcept
{
	static constexpr auto dice = 1_d10 + 2_d8;
	//static constexpr auto integ = dice.Confidence();

	auto const possib = dice.Possibilities();

	std::print("Possible combinations: {}\n", possib);
	std::print("Range: [{}-{}], => {}\n", dice.Range().first, dice.Range().second, dice.Expectation());
	std::print("\n");

	auto const perc = dice.Percentages();

	for (auto&& [what, percentage] : std::views::enumerate(perc))
		if (percentage > 0)
			std::print("{0:>2}: {1:.2f}% - {3:*<{2}}\n", what, percentage * 100, (int)std::round(percentage * 100), "");

	std::print(" - Total: {}\n", perc.size() - dice.Range().first);

	std::print("\n");
	//std::print("Chance to kill HP 50: {0:.1f}%\n", dice_t::ChanceToKill(perc, 50) * 100);
	std::print("Confidence > 70%: {}\n", dice_t::Confidence(perc, 0.7));
	std::print("Confidence > 80%: {}\n", dice_t::Confidence(perc, 0.8));
	std::print("Confidence > 90%: {}\n", dice_t::Confidence(perc, 0.9));
	std::print("Confidence absolute: {}\n", dice_t::Confidence(perc));
}
