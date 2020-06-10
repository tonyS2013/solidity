/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libyul/optimiser/MemoryEscalator.h>
#include <libyul/optimiser/CallGraphGenerator.h>
#include <libyul/optimiser/ExpressionJoiner.h>
#include <libyul/optimiser/ExpressionSplitter.h>
#include <libyul/optimiser/NameDispenser.h>
#include <libyul/backends/evm/EVMDialect.h>
#include <libyul/AsmData.h>
#include <libyul/CompilabilityChecker.h>
#include <libyul/Dialect.h>
#include <libyul/Exceptions.h>
#include <libyul/Object.h>
#include <libyul/Utilities.h>
#include <libsolutil/Algorithms.h>
#include <libsolutil/CommonData.h>
#include <libsolutil/Visitor.h>
#include <list>

using namespace std;
using namespace solidity;
using namespace solidity::yul;

namespace {
FunctionCall* findMemoryInit(Object& _object)
{
	if (!_object.code || _object.code->statements.empty())
		return nullptr;
	if (auto block = std::get_if<Block>(&_object.code->statements.front()))
		if (!block->statements.empty())
			if (auto expressionStatement = std::get_if<ExpressionStatement>(&block->statements.front()))
				if (auto functionCall = std::get_if<FunctionCall>(&expressionStatement->expression))
					if (functionCall->functionName.name == "mstore"_yulstring)
						if (
							auto mpos = std::get_if<Literal>(&functionCall->arguments.front());
							mpos && valueOfLiteral(*mpos) == 64 /* TODO: avoid hardcoding this */
						)
							return functionCall;

	return nullptr;
}

class VariableEscalator: ASTModifier
{
public:
	VariableEscalator(
		std::map<YulString, std::map<YulString, uint64_t>> const& _memorySlots,
		u256 const& _reservedMemory,
		NameDispenser& _nameDispenser
	): m_memorySlots(_memorySlots), m_reservedMemory(_reservedMemory), m_nameDispenser(_nameDispenser)
	{
	}

	using ASTModifier::operator();

	virtual void operator()(FunctionDefinition& _functionDefinition)
	{
		auto saved = m_currentFunctionMemorySlots;
		if (m_memorySlots.count(_functionDefinition.name))
		{
			m_currentFunctionMemorySlots = &m_memorySlots.at(_functionDefinition.name);
			for (auto const& param: _functionDefinition.parameters + _functionDefinition.returnVariables)
				if (m_currentFunctionMemorySlots->count(param.name))
				{
					// TODO: we cannot handle function parameters yet.
					m_currentFunctionMemorySlots = nullptr;
					break;
				}
		}
		else
			m_currentFunctionMemorySlots = nullptr;
		ASTModifier::operator()(_functionDefinition);
		m_currentFunctionMemorySlots = saved;
	}
	YulString getMemoryLocation(uint64_t _slot)
	{
		return YulString{util::toCompactHexWithPrefix(m_reservedMemory + 32 * _slot)};
	}
	static void appendMemoryStores(
		std::vector<Statement>& _statements,
		langutil::SourceLocation _loc,
		std::vector<std::pair<YulString, YulString>> const& _mstores
	)
	{
		for (auto const& [mpos, value]: _mstores)
			_statements.emplace_back(ExpressionStatement{_loc, FunctionCall{
				_loc,
				Identifier{_loc, "mstore"_yulstring},
				{
					Literal{_loc, LiteralKind::Number, mpos, {}},
					Identifier{_loc, value}
				}
			}});
	}
	virtual void operator()(Block& _block)
	{
		using OptionalStatements = std::optional<vector<Statement>>;
		if (!m_currentFunctionMemorySlots)
		{
			ASTModifier::operator()(_block);
			return;
		}
		auto containsVariableNeedingEscalation = [&](auto const& _variables) {
			return util::contains_if(_variables, [&](auto const& var) {
				return m_currentFunctionMemorySlots->count(var.name);
			});
		};

		util::iterateReplacing(
			_block.statements,
			[&](Statement& _statement)
			{
				auto defaultVisit = [&]() { ASTModifier::visit(_statement); return OptionalStatements{}; };
				return std::visit(util::GenericVisitor{
					[&](Assignment& _assignment) -> OptionalStatements
					{
						if (!containsVariableNeedingEscalation(_assignment.variableNames))
							return defaultVisit();
						visit(*_assignment.value);
						auto loc = _assignment.location;
						std::vector<Statement> result;
						if (_assignment.variableNames.size() == 1)
						{
							uint64_t slot = m_currentFunctionMemorySlots->at(_assignment.variableNames.front().name);
							result.emplace_back(ExpressionStatement{loc, FunctionCall{
								loc,
								Identifier{loc, "mstore"_yulstring},
								{
									Literal{loc, LiteralKind::Number, getMemoryLocation(slot), {}},
									std::move(*_assignment.value)
								}
							}});
						}
						else
						{
							std::vector<std::pair<YulString, YulString>> mstores;
							VariableDeclaration tempDecl{loc, {}, {}};
							for (auto& var: _assignment.variableNames)
								if (m_currentFunctionMemorySlots->count(var.name))
								{
									uint64_t slot = m_currentFunctionMemorySlots->at(var.name);
									var.name = m_nameDispenser.newName(var.name);
									mstores.emplace_back(getMemoryLocation(slot), var.name);
									tempDecl.variables.emplace_back(TypedName{loc, var.name, {}});
								}
							result.emplace_back(std::move(tempDecl));
							result.emplace_back(std::move(_assignment));
							appendMemoryStores(result, loc, mstores);
						}
						return {std::move(result)};
					},
					[&](VariableDeclaration& _varDecl) -> OptionalStatements
					{
						if (!containsVariableNeedingEscalation(_varDecl.variables))
							return defaultVisit();
						if (_varDecl.value)
							visit(*_varDecl.value);
						auto loc = _varDecl.location;
						std::vector<Statement> result;
						if (_varDecl.variables.size() == 1)
						{
							uint64_t slot = m_currentFunctionMemorySlots->at(_varDecl.variables.front().name);
							result.emplace_back(ExpressionStatement{loc, FunctionCall{
								loc,
								Identifier{loc, "mstore"_yulstring},
								{
									Literal{loc, LiteralKind::Number, getMemoryLocation(slot), {}},
									_varDecl.value ? std::move(*_varDecl.value) : Literal{loc, LiteralKind::Number, "0"_yulstring, {}}
								}
							}});
						}
						else
						{
							std::vector<std::pair<YulString, YulString>> mstores;
							for (auto& var: _varDecl.variables)
								if (m_currentFunctionMemorySlots->count(var.name))
								{
									uint64_t slot = m_currentFunctionMemorySlots->at(var.name);
									var.name = m_nameDispenser.newName(var.name);
									mstores.emplace_back(getMemoryLocation(slot), var.name);
								}
							result.emplace_back(std::move(_varDecl));
							appendMemoryStores(result, loc, mstores);
						}
						return {std::move(result)};
					},
					[&](auto&) { return defaultVisit(); }
				}, _statement);
			});
	}
	virtual void visit(Expression& _expression)
	{
		if (
			auto identifier = std::get_if<Identifier>(&_expression);
			identifier && m_currentFunctionMemorySlots && m_currentFunctionMemorySlots->count(identifier->name)
		)
		{
			auto loc = identifier->location;
			_expression = FunctionCall {
				loc,
				Identifier{loc, "mload"_yulstring}, {
					Literal {
						loc,
						LiteralKind::Number,
						getMemoryLocation(m_currentFunctionMemorySlots->at(identifier->name)),
						{}
					}
				}
			};
		}
		else
			ASTModifier::visit(_expression);
	}
private:
	std::map<YulString, std::map<YulString, uint64_t>> const& m_memorySlots;
	u256 m_reservedMemory;
	NameDispenser& m_nameDispenser;
	std::map<YulString, uint64_t> const* m_currentFunctionMemorySlots = nullptr;
};

}

void MemoryEscalator::run(OptimiserStepContext& _context, Object& _object, bool _optimizeStackAllocation)
{
	auto functionStackErrorInfo = CompilabilityChecker::run(_context.dialect, _object, _optimizeStackAllocation);
	if (functionStackErrorInfo.empty())
		return;

	yulAssert(dynamic_cast<EVMDialect const*>(&_context.dialect), "MemoryEscalator can only be run on EVMDialect objects.");
	// TODO: make this more robust with value tracking and stepping through the outermost block
	FunctionCall* memoryInit = findMemoryInit(_object);
	if (!memoryInit)
		return;
	u256 reservedMemory = 0;
	if (auto reservedMemoryLiteral = std::get_if<Literal>(&memoryInit->arguments.back()))
		reservedMemory = valueOfLiteral(*reservedMemoryLiteral);
	else
		return;

	auto callGraph = CallGraphGenerator::callGraph(*_object.code).functionCalls;

	std::set<YulString> leaves;

	// TODO: I'm assuming it doesn't contain directed cycles so far. Need to check and exit otherwise.
	std::map<YulString, set<YulString>> backEdges;
	util::BreadthFirstSearch<YulString>{{{}}}.run([&](YulString const& _node, auto&& _addChild) {
		if (callGraph.count(_node))
		{
			auto const& calledFunctions = callGraph.at(_node);
			if (calledFunctions.empty())
				leaves.emplace(_node);
			else
				for (auto const& calledFunction: callGraph.at(_node))
				{
					backEdges[calledFunction].insert(_node);
					_addChild(calledFunction);
				}
		}
		else
			leaves.emplace(_node);
	});

	std::map<YulString, std::map<YulString, uint64_t>> assignedMemorySlots;
	uint64_t requiredSlots = 0;
	{
		std::set<YulString> visited;
		std::map<YulString, uint64_t> nextAvailableSlots;
		std::list<YulString> V{leaves.begin(), leaves.end()};
		while (!V.empty())
		{
			YulString v = *V.begin();
			V.pop_front();
			if (visited.count(v))
				continue;
			visited.insert(v);
			auto const& outEdges = callGraph[v];
			uint64_t n = 0;
			for (YulString outEdge: outEdges)
				n = std::max(n, nextAvailableSlots[outEdge]);
			if (functionStackErrorInfo.count(v))
			{
				auto const& stackErrorInfo = functionStackErrorInfo.at(v);
				yulAssert(!assignedMemorySlots.count(v), "");
				auto& assignedSlots = assignedMemorySlots[v];
				for (auto const& variable: stackErrorInfo.variables)
					assignedSlots[variable] = n++;
			}
			nextAvailableSlots[v] = n;
			for (YulString inEdge: backEdges[v])
				V.push_back(inEdge);
		}
		requiredSlots = nextAvailableSlots[YulString{}];
	}

	auto loc = locationOf(memoryInit->arguments.back());
	FunctionCall memoryInitAdd{
		loc,
		Identifier{loc, "add"_yulstring},
		{
			std::move(memoryInit->arguments.back()),
			Literal{loc, LiteralKind::Number, YulString{to_string(32 * requiredSlots)}, {}}
		}
	};
	memoryInit->arguments.back() = std::move(memoryInitAdd);

	if (_object.code)
		VariableEscalator{assignedMemorySlots, reservedMemory, _context.dispenser}(*_object.code);
}