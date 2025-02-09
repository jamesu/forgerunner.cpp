#include <stdio.h>
#include "include/expressionEval.h"

using namespace ExpressionEval;

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
}

int main(int argc, char** argv)
{
   testExpr("github.event_name == 'push' && contains(github.ref, 'feature') && array[0] != 10");
   testExpr("foo.bar");
   testExpr("foo.*.bar");
   testExpr("foo.*.bar.job");
   testExpr("foo.*.bar.job == 'pie'");
	return 0;
}
