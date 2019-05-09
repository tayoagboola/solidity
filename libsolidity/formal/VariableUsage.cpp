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

#include <libsolidity/formal/VariableUsage.h>

#include <libsolidity/formal/SMTChecker.h>

#include <algorithm>

using namespace std;
using namespace dev;
using namespace dev::solidity;

set<VariableDeclaration const*> VariableUsage::touchedVariables(ASTNode const& _node, vector<CallableDeclaration const*> const& _outerCallstack)
{
	m_touchedVariables.clear();
	m_callStack.clear();
	m_callStack += _outerCallstack;
	m_lastCall = m_callStack.back();
	_node.accept(*this);
	return m_touchedVariables;
}

void VariableUsage::endVisit(Identifier const& _identifier)
{
	checkIdentifier(_identifier);
}

void VariableUsage::endVisit(MemberAccess const& _memberAccess)
{
	checkAccess(_memberAccess);
}

void VariableUsage::endVisit(IndexAccess const& _indexAccess)
{
	checkAccess(_indexAccess);
}

void VariableUsage::endVisit(FunctionCall const& _funCall)
{
	if (auto const& funDef = SMTChecker::inlinedFunctionCallToDefinition(_funCall))
		if (find(m_callStack.begin(), m_callStack.end(), funDef) == m_callStack.end())
			funDef->accept(*this);
}

bool VariableUsage::visit(FunctionDefinition const& _function)
{
	m_callStack.push_back(&_function);
	return true;
}

void VariableUsage::endVisit(FunctionDefinition const&)
{
	solAssert(!m_callStack.empty(), "");
	m_callStack.pop_back();
}

void VariableUsage::endVisit(ModifierInvocation const& _modifierInv)
{
	auto const& modifierDef = dynamic_cast<ModifierDefinition const*>(_modifierInv.name()->annotation().referencedDeclaration);
	if (modifierDef)
		modifierDef->accept(*this);
}

void VariableUsage::endVisit(PlaceholderStatement const&)
{
	solAssert(!m_callStack.empty(), "");
	FunctionDefinition const* funDef = nullptr;
	for (auto it = m_callStack.rbegin(); it != m_callStack.rend() && !funDef; ++it)
		funDef = dynamic_cast<FunctionDefinition const*>(*it);
	solAssert(funDef, "");
	if (funDef->isImplemented())
		funDef->body().accept(*this);
}

Expression const* VariableUsage::baseExpression(Expression const& _expr)
{
	if (auto memberAccess = dynamic_cast<MemberAccess const*>(&_expr))
		return &memberAccess->expression();
	if (auto indexAccess = dynamic_cast<IndexAccess const*>(&_expr))
		return &indexAccess->baseExpression();
	return nullptr;
}

void VariableUsage::checkIdentifier(Identifier const& _identifier, bool _mustLValue)
{
	Declaration const* declaration = _identifier.annotation().referencedDeclaration;
	solAssert(declaration, "");
	if (VariableDeclaration const* varDecl = dynamic_cast<VariableDeclaration const*>(declaration))
		if (_identifier.annotation().lValueRequested || !_mustLValue)
		{
			solAssert(m_lastCall, "");
			if (!varDecl->isLocalVariable() || varDecl->functionOrModifierDefinition() == m_lastCall)
				m_touchedVariables.insert(varDecl);
		}
}

void VariableUsage::checkAccess(Expression const& _expr)
{
	Expression const* base = &_expr;
	while (base && !dynamic_cast<Identifier const*>(base))
		base = baseExpression(*base);
	auto identifier = dynamic_cast<Identifier const*>(base);
	if (identifier)
		checkIdentifier(*identifier, false);
}
