#include <stdio.h>
#include "include/expressionEval.h"

using namespace ExpressionEval;
std::unordered_map<std::string, FuncInfo> ExprState::smFunctions;

struct TestKVContext : public ExprObject
{
   std::unordered_map<std::string, ExprValue> mValues;
   
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
         if (itr.second.objectInstance)
         {
            outItems.push_back(itr.second.objectInstance);
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

   Parser parser(tokens);
   CompiledStatement* stmts = parser.compile();

   for (ExprNode* node : stmts->mExpressions)
   {
      printf("%s\n", node->getName());
   }
   
   ExprState myState;
   TestKVContext* ghTest = new TestKVContext();
   TestKVContext* fooTest = new TestKVContext();
   TestKVContext* subTest = new TestKVContext();
   ExprArray* array = new ExprArray();
   ExprValue arrayVal;
   arrayVal.setObject(array);
   
   ExprValue val;
   val.setNumeric(10);
   array->mItems.push_back(val);
   
   myState.addObject(ghTest);
   myState.addObject(fooTest);
   myState.addObject(subTest);
   myState.addObject(array);
   
   myState.setContext("github", ghTest);
   myState.setContext("foo", fooTest);
   
   val.setString("push");
   ghTest->mValues["event_name"] = val;
   fooTest->mValues["list"] = arrayVal;
   
   val.setString("tab");
   fooTest->mValues["bar"] = val;
   
   val.setString("notab");
   subTest->mValues["bar"] = val;
   val.setObject(subTest);
   fooTest->mValues["cake"] = val;
   
   ExprValue result = myState.evaluate(stmts->mRoot);
   
   if (result.objectInstance)
   {
      printf("Result=(object)%s\n", result.objectInstance->toString().c_str());
   }
   else
   {
      printf("Result=%s\n", result.value.c_str());
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
      ret.setBool(!argv[0].value.empty() && !argv[1].value.empty() &&
                  argv[0].value.find(argv[1].value) != std::string::npos);
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
