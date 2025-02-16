/*

Copyright (c) 2025 James Urquhart

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

SPDX-License-Identifier: GPL-3.0-or-later

*/

#include <stdio.h>
#include <stdint.h>
#include "include/expressionEval.h"

using namespace ExpressionEval;
std::unordered_map<std::string, FuncInfo> ExprState::smFunctions;

struct TestKVContext : public ExprObject
{
   std::unordered_map<std::string, ExprValue> mValues;
   
   TestKVContext(ExprState* state) : ExprObject(state) {;}
   
   virtual ExprValue getArrayIndex(uint32_t index)
   {
      return ExprValue();
   }
   
   virtual ExprValue getMapKey(std::string key)
   {
      auto itr = mValues.find(key);
      if (itr != mValues.end())
      {
         return itr->second;
      }
      else
      {
         return ExprValue();
      }
   }
   
   inline ExprValue setMapKey(std::string key, ExprValue value)
   {
      mValues[key] = value;
      return value;
   }
   
   virtual void toList(std::vector<ExprValue>& outItems)
   {
      for (auto itr : mValues)
      {
         outItems.push_back(itr.second);
      }
   }
   
   virtual std::string toString()
   {
      return "[array]";
   }
   
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems)
   {
      for (auto itr : mValues)
      {
         if (itr.second.getObject())
         {
            outItems.push_back(itr.second.getObject());
         }
      }
   }
};

void testExpr(const char* value)
{
   std::vector<Token> tokens;
    tokenize(value, tokens);

   printf("input: %s\n", value);
   for (Token& tok : tokens)
   {
      printf("%i[%s] '%s'\n", (int)tok.type, tokToString(tok.type), tok.value.c_str());
   }
   
   StringTable st;
   Parser parser(tokens);
   parser.mStringTable = &st;
   CompiledStatement* stmts = parser.compile();

   for (ExprNode* node : stmts->mExpressions)
   {
      printf("%s\n", node->getName());
   }
   
   ExprState myState;
   
   myState.mStringTable = &st;
   
   TestKVContext* ghTest = new TestKVContext(&myState);
   TestKVContext* fooTest = new TestKVContext(&myState);
   TestKVContext* subTest = new TestKVContext(&myState);
   ExprArray* array = new ExprArray(&myState);
   ExprValue arrayVal;
   arrayVal.setObject(array);
   
   ExprValue val;
   val.setNumeric(10);
   array->mItems.push_back(val);
   
   myState.setContext("github", ghTest);
   myState.setContext("foo", fooTest);
   
   val.setString(st, "push");
   
   ghTest->mValues["event_name"] = val;
   fooTest->mValues["list"] = arrayVal;
   
   val.setString(st, "tab");
   fooTest->mValues["bar"] = val;
   
   val.setString(st, "notab");
   subTest->mValues["bar"] = val;
   val.setObject(subTest);
   fooTest->mValues["cake"] = val;
   
   ExprValue result = myState.evaluate(stmts->mRoot);
   
   if (result.getObject())
   {
      printf("Result=(object)%s\n", result.getObject()->toString().c_str());
   }
   else
   {
      printf("Result=%s\n", result.coerceString(st));
   }
   
   delete stmts;
}

int main(int argc, char** argv)
{
   // NOTE: simplified funcs for testing
   FuncInfo containsFunc;
   containsFunc.mName = "contains";
   containsFunc.mPtr = [](ExprState* state, int argc, ExprValue* argv) {
      ExprValue ret;
      if (argc != 2)
      {
         return ret;
      }
      std::string str1 = argv[0].getString();
      std::string str2 = argv[1].getString();
      ret.setBool(!str1.empty() && !str2.empty() &&
                  str1.find(str2) != std::string::npos);
      return ret;
   };
   ExprState::smFunctions["contains"] = containsFunc;
   
   testExpr("'hello' == 'hello'");
   testExpr("'hello' == 'goodbye'");
   testExpr("1 >= 10");
   testExpr("10 >= 1");
   testExpr("github.event_name == 'push' && contains(github.ref, 'feature') && foo.list[0] != 10");
   testExpr("foo.bar");
   testExpr("foo.*.bar");
   testExpr("foo.*.bar.job");
   testExpr("foo.*.bar.job == 'pie'");
	return 0;
}
