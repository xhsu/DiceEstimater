//#define SYA_AUTO_TEST
#ifdef SYA_AUTO_TEST
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif

import std.compat;

import Utility;

using std::span;
using std::string;
using std::string_view;
using std::vector;

using namespace std::literals;

namespace Op
{
	inline constexpr string_view all{ "!^*/%+-" };
	inline constexpr string_view ext{ "!^*/%+-()" };

	inline constexpr bool LeftAssoc(char c) noexcept
	{
		return "+-/*%"sv.contains(c);
	}

	inline constexpr int Preced(char c) noexcept
	{
		switch (c)
		{
		case '!':
			return 5;

		case '^':
			return 4;

		case '*':
		case '/':
		case '%':
			return 3;

		case '+':
		case '-':
			return 2;

		case '=':
			return 1;

		default:
			return 0;
		}
	}

	inline constexpr size_t ArgCount(char c) noexcept
	{
		switch (c)
		{
		case '^':
		case '*':
		case '/':
		case '%':
		case '+':
		case '-':
		case '=':
			return 2;

		case '!':
			return 1;

		default:
			return static_cast<size_t>(-1);
		}
	}

	inline constexpr bool Test(string_view s) noexcept
	{
		for (auto&& c : s)
		{
			if (all.contains(c))
				return true;
		}

		return false;
	}

	inline constexpr bool TestExt(string_view s) noexcept
	{
		for (auto&& c : s)
		{
			if (all.contains(c) || ext.contains(c))
				return true;
		}

		return false;
	}

	inline constexpr int32_t Evaluate(char c, span<int32_t> params) noexcept
	{
		int32_t ret{};

		switch (c)
		{
		case '!':
		{
			auto factorial = [](this auto&& self, int32_t n) /*#UPDATE_AT_CPP23_static_operator*/ noexcept -> int32_t { return (n == 1 || n == 0) ? 1 : self(n - 1) * n; };
			return factorial(params[0]);
		}

		case '^':
		{
			ret = 1;
			for (int32_t i = 0; i < params[1]; ++i)
				ret *= params[0];

			return ret;
		}

		case '*':
		{
			return params[0] * params[1];
		}
		case '/':
		{
			return params[0] / params[1];
		}
		case '%':
		{
			return params[0] % params[1];
		}

		case '+':
		{
			return params[0] + params[1];
		}
		case '-':
		{
			return params[0] - params[1];
		}

		default:
			std::unreachable();
		}
	}
};

CONSTEXPR vector<string_view> ShuntingYardAlgorithm(string_view s)
{
	vector<string_view> identifiers{};
	vector<string_view> ret{};
	vector<char const*> op_stack{};	// not owning!

	bool phase = false;

	for (size_t pos = s.find_first_of(Op::ext), last_pos = 0;
		pos != s.npos || last_pos != s.npos;
		phase = !phase, last_pos = pos, pos = (phase ? s.find_first_not_of(Op::ext, pos) : s.find_first_of(Op::ext, pos))
		)
	{
		if (auto const trimmed = UTIL_Trim(s.substr(last_pos, pos - last_pos)); trimmed.length())
		{
			if (phase)	// split operators. e.g. a/(b+c) must be split to 'a' '/' '(' 'b' '+' 'c' rather than 'a' '/(' 'b'
			{
				for (auto& c : trimmed)
					identifiers.emplace_back(&c, 1);
			}
			else
				identifiers.emplace_back(trimmed);
		}
	}

	for (auto&& token : identifiers)
	{
		// Is number?
		if (token.length() > 1 || !Op::TestExt(token))
		{
			ret.emplace_back(token);
		}

		// operator?
		else if (token.length() == 1 && Op::all.contains(token[0]))
		{
			auto const o1_preced = Op::Preced(token[0]);

			/*
			while (
				there is an operator o2 at the top of the operator stack which is not a left parenthesis,
				and (o2 has greater precedence than o1 or (o1 and o2 have the same precedence and o1 is left-associative))
			):
				pop o2 from the operator stack into the output queue
			push o1 onto the operator stack
			*/

			while (!op_stack.empty() && *op_stack.back() != '('
				&& (Op::Preced(*op_stack.back()) > o1_preced || (Op::Preced(*op_stack.back()) == o1_preced && Op::LeftAssoc(token[0])))
				)
			{
				ret.emplace_back(op_stack.back(), 1);
				op_stack.pop_back();
			}

			op_stack.push_back(&token[0]);
		}

		// (
		else if (token.length() == 1 && token[0] == '(')
			op_stack.push_back("(");

		// )
		else if (token.length() == 1 && token[0] == ')')
		{
			try
			{
				while (*op_stack.back() != '(')
				{
					// pop the operator from the operator stack into the output queue

					ret.emplace_back(op_stack.back(), 1);
					op_stack.pop_back();
				}

				// pop the left parenthesis from the operator stack and discard it
				op_stack.pop_back();
			}
			catch (...)
			{
				/* If the stack runs out without finding a left parenthesis, then there are mismatched parentheses. */
			}

			/*
			if there is a function token at the top of the operator stack, then:
				pop the function from the operator stack into the output queue
			*/
		}

		else
			throw std::invalid_argument{ "Unreconsized symbol." };
	}

	/* After the while loop, pop the remaining items from the operator stack into the output queue. */
	while (!op_stack.empty())
	{
		if (*op_stack.back() == '(')
			throw std::invalid_argument{ "If the operator token on the top of the stack is a parenthesis, then there are mismatched parentheses." };

		ret.emplace_back(op_stack.back(), 1);
		op_stack.pop_back();
	}

	return ret;
}

CONSTEXPR int32_t PostfixNotationEval(vector<string_view> const& identifiers) noexcept
{
	vector<int32_t> num_stack{};

	for (auto&& token : identifiers)
	{
		if (token.length() > 1 || !Op::Test(token))
		{
			num_stack.push_back(UTIL_StrToNum<int32_t>(token));
		}
		else if (Op::Test(token))
		{
			auto const arg_count = Op::ArgCount(token[0]);
			auto const first_param_pos = num_stack.size() - arg_count;
			auto const params = span{ num_stack.data() + first_param_pos, arg_count };

			auto const res = Op::Evaluate(token[0], params);

			num_stack.erase(num_stack.begin() + first_param_pos, num_stack.end());
			num_stack.push_back(res);
		}
		else
			std::unreachable();
	}

	return num_stack.front();
}

#ifdef SYA_AUTO_TEST

consteval bool SYA_Test() noexcept
{
	auto const rpn =
		ShuntingYardAlgorithm("3+4*2/(1-5)^2^3")
		| std::views::join
		| std::ranges::to<string>();

	return rpn == "342*15-23^^/+";
}

static_assert(SYA_Test());

consteval bool PN_Test() noexcept
{
	auto const rpn = ShuntingYardAlgorithm("3*8/2^3+6^2!-1");
	return PostfixNotationEval(rpn) == 38;
}

static_assert(PN_Test());

#endif
