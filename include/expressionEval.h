#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <utility>
#include <string.h>

namespace ExpressionEval
{

struct ExprObject;
struct ExprState;
struct ExprValue;
struct ExprNode;

enum class TokenType
{
   IDENTIFIER,
   OP_NE, OP_EQ, OP_LTE, OP_GTE, OP_GT, OP_LT, OP_AND, OP_OR, OP_NOT,
   OP_MUL,
   STRING, NUMBER, LPAREN, RPAREN, LBRACKET, RBRACKET, COMMA, DOT, END
};

enum class OperatorType
{
   OP_NE, OP_EQ, OP_LTE, OP_GTE, OP_GT, OP_LT, OP_AND, OP_OR, OP_NOT,
   OP_MUL
};

// Lexer token
struct Token {
   TokenType type;
   std::string value;
   uint32_t offset;
};

struct FuncInfo
{
   typedef ExprValue (*FuncPtr)(ExprState*, int, ExprValue**);
   
   const char* mName;
   FuncPtr mPtr;
};

// Duck typed value
struct ExprValue
{
   std::string value;
   int64_t numValue;
   bool isNumeric;
   ExprObject* objectInstance;
   
   ExprValue();
   
   void setBool(bool val);
   void setNumeric(int64_t val);
   void setString(const char* str);
   void setObject(ExprObject* obj);
   bool testEq(const ExprValue& other) const;
};

// Runtime state
struct ExprState
{
   std::vector<ExprObject*> mObjects;
   std::unordered_map<std::string, ExprValue> mContexts;
   static std::unordered_map<std::string, FuncInfo> mFunctions;
   ExprValue mValue;
   
   void addObject(ExprObject* obj);
   ExprValue callFunction(const char* name, int argc, ExprValue* args);
   ExprValue evaluate(ExprNode* root);
   void setContext(const char* name, ExprObject* obj);
   ExprValue getContext(const char* name);
   
   ExprState();
   ~ExprState();
   
   void clear();
   static void init();
};

// Wrapper for an object, typically a map iterator
struct ExprObject
{
   virtual ~ExprObject();
   virtual ExprValue getArrayIndex(uint32_t index) = 0;
   virtual ExprValue getMapKey(std::string key) = 0;
   virtual void toList(std::vector<ExprValue>& outItems) = 0;
   virtual std::string toString() = 0;
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems) = 0;
};

// Array object (for *)
struct ExprArray : public ExprObject
{
   std::vector<ExprValue> mItems;
   
   virtual ExprValue getArrayIndex(uint32_t index);
   virtual ExprValue getMapKey(std::string key);
   virtual void toList(std::vector<ExprValue>& outItems);
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems);
   virtual std::string toString();
};

// AST node
struct ExprNode
{
   ExprNode* mNext;
   
   ExprNode();
   virtual ~ExprNode() = default;
   virtual ExprValue evaluate(ExprState& state) = 0;
   virtual const char* getName() { return "NONE"; }
};

struct OperatorNode : ExprNode
{
   OperatorType mOp;
   ExprNode* mLeft;
   ExprNode* mRight;
   
   OperatorNode(OperatorType op, ExprNode* left, ExprNode* right);
   
   ExprValue evaluate(ExprState& state) override;
   
   const char* getName() override { return "OP"; }
};

struct KeyAccessNode : ExprNode
{
   ExprNode* mObject;
   std::string mKey;
   
   KeyAccessNode(ExprNode* object, std::string key);
   
   ExprValue evaluate(ExprState& state) override;
   
   const char* getName() override { return "KEYACCESS"; }
};

struct FunctionCallNode : ExprNode
{
   std::string mName;
   ExprNode* mArgs;
   
   FunctionCallNode(std::string function, ExprNode* args);
   
   ExprValue evaluate(ExprState& state) override;
   
   const char* getName() override { return "FUNCTION"; }
};

// Handles key.*.value
struct ExpandoNode : ExprNode
{
   ExprNode* mLeft;
   ExprNode* mRight;
   
   ExpandoNode(ExprNode* left, ExprNode* right);
   
   ExprValue evaluate(ExprState& state) override;

   const char* getName() override { return "EXPANDO"; }
};

struct ArrayAccessNode : ExprNode
{
   ExprNode* mArray;
   ExprNode* mIndex;
   
   ArrayAccessNode(ExprNode* array, ExprNode* index);
   
   ExprValue evaluate(ExprState& state) override;
   
   const char* getName() override { return "ARRAYACCESS"; }
};

struct IdentifierNode : ExprNode
{
   std::string mName;
   
   IdentifierNode(std::string name);
   
   ExprValue evaluate(ExprState& state) override;
   
   const char* getName() override { return "IDENTIFIER"; }
};

struct LiteralNode : ExprNode
{
   ExprValue mValue;
   
   LiteralNode(ExprValue value);
   
   ExprValue evaluate(ExprState& state) override;
   
   const char* getName() override { return "LITERAL"; }
};

struct CompiledStatement
{
   std::vector<ExprNode*> mExpressions;
   ExprNode* mRoot;
   
   CompiledStatement();
   ~CompiledStatement();
   
   void clear();
   
   inline ExprNode* getRoot() { return mRoot; }
};

class Parser
{
   std::vector<Token> mTokens;
   size_t mCurrent;
   CompiledStatement* mOutput;
   
public:
   Parser(std::vector<Token> tokens);
   
   int getPrecedence(TokenType op);
   
   CompiledStatement* compile();
   ExprNode* parseExpression();
   ExprNode* parsePrimary();
   ExprNode* parseBinaryOp(int precedence);
   ExprNode* parseFunctionCall(const char* name);
   ExprNode* parseArrayAccess(ExprNode* root);
   ExprNode* parseDotAccess(ExprNode* root);
   
   template <typename T, typename... Args>
   T* allocNode(Args&&... args)
   {
      T* ret = new T(std::forward<Args>(args)...);
      mOutput->mExpressions.push_back(ret);
      return ret;
   }
   
private:
   inline Token peek() { return mTokens[mCurrent]; }
   inline Token advance() { return mTokens[mCurrent++]; }
   inline bool match(TokenType type) { return peek().type == type; }
};

inline bool exprStringToI64(const char* start, int64_t& value)
{
   value = 0;
   
   if (*start == '\0')
      return false;
   
   char* end = NULL;
   
   int64_t result = std::strtoll(start, &end, 10);
   
   while (*end && std::isspace(*end))
   {
      ++end;
   }
   
   if (*end != '\0')
   {
      return false;
   }
   
   value = result;
   return true;
}

inline const char* tokToString(TokenType type)
{
   static const char* lookupTable[] = {
      "IDENTIFIER",
      "OP_NE", "OP_EQ", "OP_LTE", "OP_GTE", "OP_GT", "OP_LT", "OP_AND", "OP_OR", "OP_NOT",
      "OP_MUL",
      "STRING", "NUMBER", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET", "COMMA", "DOT", "END"
   };
   
   return lookupTable[(int)type];
}

inline bool tokenize(const std::string &expression, std::vector<Token>& tokens)
{
   size_t i = 0;
   
   struct CodeMap
   {
      const char* code;
      TokenType token;
   };
   
   CodeMap opTable[] = {
      {".", TokenType::DOT},
      {"*", TokenType::OP_MUL},
      {"!", TokenType::OP_NOT},
      {"<", TokenType::OP_LT},
      {">", TokenType::OP_GT},
      {"[", TokenType::LBRACKET},
      {"]", TokenType::RBRACKET},
      {"(", TokenType::LPAREN},
      {")", TokenType::RPAREN},
      {",", TokenType::COMMA},
   };
   
   CodeMap op2Table[] = {
      {"==", TokenType::OP_EQ},
      {"!=", TokenType::OP_NE},
      {"<=", TokenType::OP_LTE},
      {">=", TokenType::OP_GTE},
      {"&&", TokenType::OP_AND},
      {"||", TokenType::OP_OR},
   };
   
   const size_t NumOp = sizeof(opTable) / sizeof(opTable[0]);
   const size_t NumOp2 = sizeof(op2Table) / sizeof(op2Table[0]);
   
   tokens.clear();
   
   for (size_t i=0; i < expression.size(); i++)
   {
      char code = expression[i];
      
      if (!std::isspace(code))
      {
         if (std::isalpha(code) || code == '_')
         {
            size_t start = i;
            for (; i < expression.size(); i++)
            {
               if (!(std::isalnum(expression[i]) || expression[i] == '_'))
                  break;
            }
            
            tokens.push_back({TokenType::IDENTIFIER, expression.substr(start, i - start), (uint32_t)i});
            i--;
         }
         else if (std::isdigit(code))
         {
            size_t start = i++;
            for (; i < expression.size(); i++)
            {
               if (!std::isdigit(expression[i]))
                  break;
            }
            tokens.push_back({TokenType::NUMBER, expression.substr(start, i - start), (uint32_t)i});
            i--;
         }
         else if (code == '"' || code == '\'')
         {
            char quote = code;
            size_t start = ++i;
            for (; i < expression.size(); i++)
            {
               if (expression[i] == quote)
                  break;
            }
            tokens.push_back({TokenType::STRING, expression.substr(start, i - start), (uint32_t)i});
         }
         else
         {
            // Check single & double tokens
            const char* fwdC = expression.c_str() + i;
            bool found = false;
            
            for (uint32_t k=0; k<NumOp2; k++)
            {
               if (memcmp(op2Table[k].code, fwdC, 2) == 0)
               {
                  tokens.push_back({op2Table[k].token, expression.substr(i, 2), (uint32_t)i});
                  i++;
                  found = true;
                  break;
               }
            }
            
            if (!found)
            {
               for (uint32_t k=0; k<NumOp; k++)
               {
                  if (*opTable[k].code == *fwdC)
                  {
                     tokens.push_back({opTable[k].token, expression.substr(i, 1), (uint32_t)i});
                     found = true;
                     break;
                  }
               }
            }
            
            if (!found)
            {
               std::cerr << "Unexpected character: " << expression[i] << std::endl;
               tokens.clear();
               return false;
            }
         }
      }
   }
   
   tokens.push_back({TokenType::END, "", (uint32_t)expression.size()});
   return true;
}

inline ExprValue::ExprValue() : value(""), numValue(0), isNumeric(true), objectInstance(NULL)
{
}

inline void ExprValue::setBool(bool val)
{
   value = val ? "1" : "0";
   numValue = (int64_t)val;
   isNumeric = true;
   objectInstance = NULL;
}

inline void ExprValue::setNumeric(int64_t val)
{
   value = std::to_string(val);
   numValue = (int64_t)val;
   isNumeric = true;
   objectInstance = NULL;
}

inline void ExprValue::setString(const char* str)
{
   value = str;
   isNumeric = exprStringToI64(str, numValue);
   objectInstance = NULL;
}

inline void ExprValue::setObject(ExprObject* obj)
{
   setString(obj->toString().c_str());
   objectInstance = obj;
}

inline bool ExprValue::testEq(const ExprValue& other) const
{
   if (isNumeric || other.isNumeric)
   {
      return numValue == other.numValue;
   }
   else
   {
      return strcasecmp(value.c_str(), other.value.c_str()) == 0;
   }
}

inline ExprState::ExprState()
{
}

inline ExprState::~ExprState()
{
   clear();
}

inline void ExprState::addObject(ExprObject* obj)
{
   mObjects.push_back(obj);
}

inline ExprValue ExprState::callFunction(const char* name, int argc, ExprValue* args)
{
   return ExprValue(); // TODO
}

inline ExprValue ExprState::evaluate(ExprNode* root)
{
   return ExprValue(); // TODO
}

inline void ExprState::setContext(const char* name, ExprObject* obj)
{
    // TODO
}

inline ExprValue ExprState::getContext(const char* name)
{
   return ExprValue(); // TODO
}

inline void ExprState::clear()
{
   for (ExprObject* obj : mObjects)
   {
      delete obj;
   }
   mObjects.clear();
}

inline ExprObject::~ExprObject()
{
}

inline ExprValue ExprArray::getArrayIndex(uint32_t index)
{
  return mItems[index];
}

inline ExprValue ExprArray::getMapKey(std::string key)
{
  return ExprValue();
}

inline void ExprArray::toList(std::vector<ExprValue>& outItems)
{
  outItems = mItems;
}

inline void ExprArray::getExpandRoots(std::vector<ExprObject*>& outItems)
{
  for (ExprValue& value : mItems)
  {
     if (value.objectInstance)
     {
        outItems.push_back(value.objectInstance);
     }
  }
}

inline std::string ExprArray::toString()
{
  std::string outS = "";
  size_t count = 0;
  for (ExprValue& item : mItems)
  {
     outS += item.value;
     if (count+1 < mItems.size())
        outS += " ";
     count++;
  }
  return outS;
}

inline ExprNode::ExprNode() : mNext(NULL)
{
}

inline OperatorNode::OperatorNode(OperatorType op, ExprNode* left, ExprNode* right)
: mOp(op), mLeft(left), mRight(right)
{
}

inline ExprValue OperatorNode::evaluate(ExprState& state)
{
  ExprValue val;
  
  switch (mOp)
  {
     case OperatorType::OP_NE:
        val.setBool(!mLeft->evaluate(state).testEq(mRight->evaluate(state)));
        break;
     case OperatorType::OP_EQ:
        val.setBool(mLeft->evaluate(state).testEq(mRight->evaluate(state)));
        break;
     case OperatorType::OP_LTE:
        val.setBool(mLeft->evaluate(state).numValue <= mRight->evaluate(state).numValue);
        break;
     case OperatorType::OP_GTE:
        val.setBool(mLeft->evaluate(state).numValue >= mRight->evaluate(state).numValue);
        break;
     case OperatorType::OP_GT:
        val.setBool(mLeft->evaluate(state).numValue > mRight->evaluate(state).numValue);
        break;
     case OperatorType::OP_LT:
        val.setBool(mLeft->evaluate(state).numValue < mRight->evaluate(state).numValue);
        break;
     case OperatorType::OP_AND:
        val.setBool(mLeft->evaluate(state).numValue && mRight->evaluate(state).numValue);
        break;
     case OperatorType::OP_OR:
        val.setBool(mLeft->evaluate(state).numValue || mRight->evaluate(state).numValue);
        break;
     case OperatorType::OP_NOT:
        val.setBool(!((bool)mLeft->evaluate(state).numValue));
        break;
     case OperatorType::OP_MUL:
        val.setNumeric(mLeft->evaluate(state).numValue * mRight->evaluate(state).numValue);
        break;
     default:
        break;
  }
  
  return val;
}

inline KeyAccessNode::KeyAccessNode(ExprNode* object, std::string key)
: mObject(object), mKey(std::move(key))
{
}

inline ExprValue KeyAccessNode::evaluate(ExprState& state)
{
   ExprValue mapObject = mObject->evaluate(state);
   if (mapObject.objectInstance == NULL)
   {
      throw std::runtime_error("Map object is not present");
   }
   return mapObject.objectInstance->getMapKey(mKey);
}

inline FunctionCallNode::FunctionCallNode(std::string function, ExprNode* args)
: mName(std::move(function)), mArgs(args)
{
}

inline ExprValue FunctionCallNode::evaluate(ExprState& state)
{
   uint32_t count = 0;
   std::vector<ExprValue> argVector;
   
   for (ExprNode* ptr = mArgs; ptr; ptr = ptr->mNext)
   {
      argVector.push_back(ptr->evaluate(state));
   }
   
   return state.callFunction(mName.c_str(), (int)argVector.size(), &argVector[0]);
}

inline ExpandoNode::ExpandoNode(ExprNode* left, ExprNode* right)
: mLeft(left), mRight(right)
{
}

inline ExprValue ExpandoNode::evaluate(ExprState& state)
{
   ExprValue baseObject = mLeft->evaluate(state);
   
   std::vector<ExprObject*> expandRoots;
   if (baseObject.objectInstance == NULL)
   {
      throw std::runtime_error("Not an object");
   }
   
   ExprArray* array = new ExprArray();
   ExprValue retVal;
   retVal.setObject(array);
   
   baseObject.objectInstance->getExpandRoots(expandRoots);
   ExprValue expandKey = mRight->evaluate(state);
   for (ExprObject* obj : expandRoots)
   {
      ExprValue value = obj->getMapKey(expandKey.value);
      if (!value.value.empty())
      {
         array->mItems.push_back(value);
      }
   }
   
   return retVal;
}

inline ArrayAccessNode::ArrayAccessNode(ExprNode* array, ExprNode* index)
: mArray(array), mIndex(index)
{
}

inline ExprValue ArrayAccessNode::evaluate(ExprState& state)
{
   ExprValue index = mIndex->evaluate(state);
   if (!index.isNumeric)
   {
      throw std::runtime_error("Array accessor is not numeric");
   }
   ExprValue arrayObject = mArray->evaluate(state);
   if (arrayObject.objectInstance == NULL)
   {
      throw std::runtime_error("Array object is not present");
   }
   return arrayObject.objectInstance->getArrayIndex((uint32_t)index.numValue);
}

inline IdentifierNode::IdentifierNode(std::string name) : mName(std::move(name))
{
}

inline ExprValue IdentifierNode::evaluate(ExprState& state)
{
   return state.getContext(mName.c_str());
}

inline LiteralNode::LiteralNode(ExprValue value) : mValue(value)
{
}

inline ExprValue LiteralNode::evaluate(ExprState& state)
{
   return mValue;
}

inline CompiledStatement::CompiledStatement() : mRoot(NULL)
{
}

inline CompiledStatement::~CompiledStatement()
{
   clear();
}

inline void CompiledStatement::clear()
{
   for (ExprNode* node : mExpressions)
      delete node;
   mExpressions.clear();
}

inline Parser::Parser(std::vector<Token> tokens) : mTokens(std::move(tokens)), mCurrent(0)
{ 
}

inline int Parser::getPrecedence(TokenType op)
{
   switch (op)
   {
      case TokenType::OP_OR: return 1;
      case TokenType::OP_AND: return 2;
      case TokenType::OP_EQ:
      case TokenType::OP_NE: return 3;
      case TokenType::OP_LT:
      case TokenType::OP_LTE:
      case TokenType::OP_GT:
      case TokenType::OP_GTE: return 4;
      case TokenType::OP_MUL: return 5;
      default: return 0;
   }
}

inline CompiledStatement* Parser::compile()
{
   mOutput = new CompiledStatement();
   mOutput->mRoot = parseExpression();
   
   if (mOutput->mRoot == NULL)
   {
      delete mOutput;
      mOutput = NULL;
      return NULL;
   }
   
   return mOutput;
}

inline ExprNode* Parser::parseExpression()
{
   if (mTokens.empty())
      return NULL;
   return parseBinaryOp(0);
}

// i.e. foo + woo
inline ExprNode* Parser::parseBinaryOp(int precedence)
{
   ExprNode* left = parsePrimary();
   if (left == NULL)
   {
      printf("Expression expected\n");
      return NULL;
   }
   
   while (true)
   {
      TokenType op = peek().type;
      int opPrecedence = getPrecedence(op);
      
      if (op == TokenType::OP_MUL)
      {
         printf("* not supported by itself\n");
         return NULL;
      }
      
      if (opPrecedence == 0 || opPrecedence < precedence)
         break;
      
      advance();
      ExprNode* right = parseBinaryOp(opPrecedence + 1);
      left = allocNode<OperatorNode>(static_cast<OperatorType>((int)op - (int)TokenType::OP_NE), left, right);
   }
   
   return left;
}

// Consumes everything that is
// key.access
// array[access]
// "literals"
// (paren)
// ! expr
inline ExprNode* Parser::parsePrimary()
{
   Token token = advance();
   
   if (token.type == TokenType::NUMBER ||
       token.type == TokenType::STRING)
   {
      ExprValue value;
      value.setString(token.value.c_str());
      return allocNode<LiteralNode>(value);
   }
   else if (token.type == TokenType::IDENTIFIER)
   {
      if (match(TokenType::LPAREN))
      {
         return parseFunctionCall(token.value.c_str());
      }
      else if (match(TokenType::LBRACKET))
      {
         return parseArrayAccess(allocNode<IdentifierNode>(token.value));
      }
      else if (match(TokenType::DOT))
      {
         return parseDotAccess(allocNode<IdentifierNode>(token.value));
      }
      else
      {
         return allocNode<IdentifierNode>(token.value);
      }
   }
   else if (token.type == TokenType::LPAREN)
   {
      ExprNode* expr = parseExpression();
      if (token.type != TokenType::RPAREN)
      {
         printf("Sub-expression doesn't end in )\n");
         return NULL;
      }
      advance(); // Consume ')'
      return expr;
   }
   else if (token.type == TokenType::OP_NOT)
   {
      ExprNode* operand = parsePrimary();
      return allocNode<OperatorNode>(OperatorType::OP_NOT, operand, nullptr);
   }
   
   return NULL;
}

// i.e. func(a, b, c)
inline ExprNode* Parser::parseFunctionCall(const char* function)
{
   ExprNode* args = NULL;
   advance(); // Consume '('
   
   while (!match(TokenType::RPAREN))
   {
      if (match(TokenType::COMMA))
      {
         advance(); // Consume ','
         continue;
      }
      
      if (args)
      {
         args->mNext = parseExpression();
      }
      else
      {
         args = parseExpression();
      }
   }
   
   advance(); // Consume ')'
   return allocNode<FunctionCallNode>(function, args);
}

// i.e. foo[0] foo.woo[1] BUT NOT foo.*.woo[0]
inline ExprNode* Parser::parseArrayAccess(ExprNode* primary)
{
   while (match(TokenType::LBRACKET))
   {
      advance(); // Consume '['
      ExprNode* index = parseExpression();
      advance(); // Consume ']'
      primary = allocNode<ArrayAccessNode>(primary, index);
   }
   
   return primary;
}

// i.e. foo.woo
inline ExprNode* Parser::parseDotAccess(ExprNode* primary)
{
   while (match(TokenType::DOT))
   {
      advance(); // Consume '.'
      
      if (match(TokenType::DOT))
      {
         printf("Too many dots\n");
         return NULL;
      }
      if (match(TokenType::IDENTIFIER))
      {
         primary = allocNode<KeyAccessNode>(primary, advance().value);
      }
      else if (match(TokenType::OP_MUL))
      {
         std::string rightKey;
         advance(); // consume '*'
         
         if (match(TokenType::DOT))
         {
            advance(); // consume '.'
            while (match(TokenType::IDENTIFIER))
            {
               rightKey += advance().value;
               if (!match(TokenType::DOT))
               {
                  break;
               }
               rightKey += ".";
               advance(); // consume '.'
            }
         }
         else
         {
            // NEEDS to be identifier
            printf("Key identifier MUST come after *\n");
            return NULL;
         }
         
         primary = allocNode<ExpandoNode>(primary, new IdentifierNode(rightKey));
      }
      else
      {
         break;
      }
   }
   
   return primary;
}

}
