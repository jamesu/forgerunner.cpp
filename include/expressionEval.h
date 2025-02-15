#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <cstddef>
#include <utility>
#include <string.h>
#include <stdint.h>
#include <unordered_set>
#include <google/protobuf/util/json_util.h>
#include <fkYAML/node.hpp>

namespace ExpressionEval
{

struct ExprObject;
struct ExprState;
struct ExprValue;
struct ExprNode;
class StringTable;

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
   typedef ExprValue (*FuncPtr)(ExprState*, int, ExprValue*);
   
   const char* mName;
   FuncPtr mPtr;
};

class StringTable
{
public:
   const char* intern(const char* str);

private:
   std::unordered_set<std::string> table;
};

// Duck typed value
struct ExprValue
{
   enum Tag : uint64_t
   {
      TAG_SHIFT = 50,
      NUMBER = 0x0ULL << TAG_SHIFT,
      BOOLEAN = 0x1ULL << TAG_SHIFT,
      STRING = 0x2ULL << TAG_SHIFT,
      OBJECT = 0x3ULL << TAG_SHIFT,
      PAYLOAD_MASK = 0x0003FFFFFFFFFFFFULL,
      NAN_MASK = 0x7FF0000000000000ULL,
      TAG_MASK = 0x3ULL << TAG_SHIFT
   };
   
   inline bool isNull() const { return ((value & (NAN_MASK | OBJECT)) != 0) ? (getObject() != NULL) : false; }

   uint64_t value;
   
   ExprValue();
   
   ExprValue& setBool(bool val);
   ExprValue& setNumeric(float64_t val);
   ExprValue& setString(StringTable& st, const char* str);
   ExprValue& setObject(ExprObject* obj);
   
   ExprObject* getObject() const;
   template<class T> T* asObject() { return dynamic_cast<T*>(getObject()); }
   bool getBool() const;
   float64_t getNumber() const;
   const char* getString() const;
   const char* getStringSafe() const;
   
   inline bool isNumber() const { return (value & NAN_MASK) == 0; }
   inline bool isBool() const { return !isNumber() && (value & TAG_MASK) == BOOLEAN; }
   inline bool isString() const { return !isNumber() && (value & TAG_MASK) == STRING; }
   inline bool isObject() const { return !isNumber() && (value & TAG_MASK) == OBJECT; }
   
   const char* coerceString(StringTable& st) const;
   
   bool testEq(const ExprValue& other) const;
};

// Runtime state
struct ExprState
{
   enum
   {
      MaxStackDepth = 3
   };
   
   StringTable* mStringTable;
   std::vector<ExprObject*> mObjects;
   std::unordered_map<std::string, ExprObject*> mContexts;
   static std::unordered_map<std::string, FuncInfo> smFunctions;
   ExprValue mValue;
   int8_t mDepth;
   
   ExprObject* addObject(ExprObject* obj);
   ExprValue callFunction(const char* name, int argc, ExprValue* args);
   ExprValue evaluate(ExprNode* root);
   void setContext(const char* name, ExprObject* obj);
   ExprObject* getContext(const char* name);
   std::string substituteExpressions(const std::string &input, int8_t depth=MaxStackDepth);
   ExprValue substituteSingleExpression(const std::string &input, int8_t depth=MaxStackDepth);
   ExprValue evaluateString(const std::string &input);
   
   ExprState();
   ~ExprState();
   
   void clear();
   static void init();
};

// Wrapper for an object, typically a map iterator
struct ExprObject
{
   typedef ExprObject* (*AddObjectFuncPtr)(ExprState*);
   ExprState* mState;
   AddObjectFuncPtr mAddObjectFunc;
   
   ExprObject(ExprState* state);
   virtual ~ExprObject();
   
   virtual void clear() = 0;
   inline ExprObject* constructObject();
   virtual void addArrayValue(ExprValue value) = 0;
   virtual ExprValue getArrayIndex(uint32_t index) = 0;
   virtual ExprValue getMapKey(std::string key) = 0;
   virtual ExprValue setMapKey(std::string key, ExprValue value) = 0;
   virtual void toList(std::vector<ExprValue>& outItems) = 0;
   virtual void extractKeys(std::vector<std::string>& outKeys) = 0;
   virtual std::string toString() = 0;
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems) = 0;
   
   template<typename T> static ExprObject* addTypedObjectFunc(ExprState* state)
   {
      return new T(state);
   }
};

// Array object (for *)
struct ExprArray : public ExprObject
{
   std::vector<ExprValue> mItems;
   
   ExprArray(ExprState* state);
   
   virtual void clear();
   virtual void addArrayValue(ExprValue value);
   virtual ExprValue getArrayIndex(uint32_t index);
   virtual ExprValue getMapKey(std::string key);
   virtual ExprValue setMapKey(std::string key, ExprValue value);
   virtual void toList(std::vector<ExprValue>& outItems);
   virtual void extractKeys(std::vector<std::string>& outKeys);
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems);
   virtual std::string toString();
};

// Generic map object
struct ExprMap : public ExprObject
{
   std::unordered_map<std::string, ExprValue> mItems;
   
   ExprMap(ExprState* state);
   virtual void clear();
   virtual void addArrayValue(ExprValue value);
   virtual ExprValue getArrayIndex(uint32_t index);
   virtual ExprValue getMapKey(std::string key);
   virtual ExprValue setMapKey(std::string key, ExprValue value);
   virtual void toList(std::vector<ExprValue>& outItems);
   virtual void extractKeys(std::vector<std::string>& outKeys);
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems);
   virtual std::string toString();
};

struct ExprMultiKey : public ExprObject
{
   ExprObject* mSlots[3];
   
   ExprMultiKey(ExprState* state);
   virtual ~ExprMultiKey();
   
   virtual void clear();
   inline ExprObject* constructObject();
   virtual void addArrayValue(ExprValue value);
   virtual ExprValue getArrayIndex(uint32_t index);
   virtual ExprValue getMapKey(std::string key);
   virtual ExprValue setMapKey(std::string key, ExprValue value);
   virtual void toList(std::vector<ExprValue>& outItems);
   virtual void extractKeys(std::vector<std::string>& outKeys);
   virtual std::string toString();
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems);
};

// Map object with fixed fields
struct ExprFieldObject : public ExprObject
{
   struct FieldRef
   {
      const char* baseName;
      uintptr_t offset;
      uint64_t typeMask;
      bool canSet;
   };
   
   template<class T> static std::unordered_map<std::string, FieldRef>& getFieldRegistry()
   {
      static std::unordered_map<std::string, FieldRef> reg;
      return reg;
   }
   
   std::unordered_map<std::string, FieldRef> mItems;
   
   ExprFieldObject(ExprState* state);
   
   virtual void clear();
   virtual void addArrayValue(ExprValue value);
   virtual ExprValue getArrayIndex(uint32_t index);
   virtual ExprValue getMapKey(std::string key);
   virtual ExprValue setMapKey(std::string key, ExprValue value);
   virtual void toList(std::vector<ExprValue>& outItems);
   virtual void extractKeys(std::vector<std::string>& outKeys);
   virtual void getExpandRoots(std::vector<ExprObject*>& outItems);
   virtual std::string toString();
   virtual std::unordered_map<std::string, FieldRef>& getObjectFieldRegistry() { return getFieldRegistry<ExprObject>(); }
   
   template<class T> static void registerFieldsForType();
   
   template <class T>
   static void registerField(const char* name, uintptr_t offset, uint64_t typeMask, bool canSet=true)
   {
      auto& fields = getFieldRegistry<T>();
      if ((typeMask & ExprValue::TAG_MASK) != 0)
      {
         typeMask |= ExprValue::NAN_MASK;
      }
      for (auto& itr : fields)
      {
         if (itr.second.offset == offset)
         {
            throw std::runtime_error("Conflicting offsets");
         }
      }
      fields[std::string(name)] = { name, offset, typeMask, canSet };
   }
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
   StringTable* mStringTable;
   
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

inline const char* StringTable::intern(const char* str)
{
      auto it = table.insert(str).first;
      return it->c_str();
}

inline ExprValue::ExprValue() : value(ExprValue::NAN_MASK | ExprValue::OBJECT)
{
}

inline ExprValue& ExprValue::setBool(bool val)
{
   value = ExprValue::NAN_MASK | ExprValue::BOOLEAN | (uint64_t)val;
   return *this;
}

inline ExprValue& ExprValue::setNumeric(float64_t val)
{
   value = ExprValue::NUMBER | (((uint64_t)(uintptr_t)val) & PAYLOAD_MASK);
   return *this;
}

inline ExprValue& ExprValue::setString(StringTable& st, const char* str)
{
   const char *strVal = st.intern(str);
   value = ExprValue::NAN_MASK | ExprValue::STRING | ((uint64_t)(uintptr_t)strVal & PAYLOAD_MASK);
   return *this;
}

inline ExprValue& ExprValue::setObject(ExprObject* obj)
{
   value = ExprValue::NAN_MASK | ExprValue::OBJECT | ((uint64_t)(uintptr_t)obj & PAYLOAD_MASK);
   return *this;
}

inline ExprObject* ExprValue::getObject() const
{
   const uint64_t checkVal = (ExprValue::NAN_MASK | ExprValue::OBJECT);
   if ((value & checkVal) == checkVal)
      return (ExprObject*)(value & ExprValue::PAYLOAD_MASK);
   else
      return NULL;
}

inline bool ExprValue::getBool() const
{
   const uint64_t checkVal = (ExprValue::NAN_MASK | ExprValue::BOOLEAN);
   return ((value & checkVal) == checkVal) ? (uint64_t)(value & ExprValue::PAYLOAD_MASK) != 0 : false;
}

inline float64_t ExprValue::getNumber() const
{
   if ((value & ExprValue::STRING) != 0)
   {
      return std::stold(getString());
   }
   else if ((value & ExprValue::BOOLEAN) != 0)
   {
      return getBool() ? 1.0 : 0.0;
   }
   else if ((value & ExprValue::OBJECT) != 0)
   {
      return getObject() ? std::numeric_limits<float64_t>::quiet_NaN() : 0.0;
   }
   else
   {
      return (float64_t)(value & ExprValue::PAYLOAD_MASK);
   }
}

inline const char* ExprValue::getString() const
{
   const uint64_t checkVal = (ExprValue::NAN_MASK | ExprValue::STRING);
   return ((value & checkVal) == checkVal) ? (const char*)(value & ExprValue::PAYLOAD_MASK) : NULL;
}

inline const char* ExprValue::getStringSafe() const
{
   const char* ret = getString();
   return ret ? ret : "";
}


const char* ExprValue::coerceString(StringTable& st) const
{
   if (isString())
   {
      return getString();
   }
   else if (isBool())
   {
      return getBool() ? "true" : "false";
   }
   else if (isObject())
   {
      return getObject() ? "NaN" : "null";
   }
   else
   {
      return st.intern(std::to_string(getNumber()).c_str());
   }
}

inline bool ExprValue::testEq(const ExprValue& other) const
{
   if ((value & (ExprValue::TAG_MASK | ExprValue::NAN_MASK)) == (other.value & (ExprValue::TAG_MASK | ExprValue::NAN_MASK)))
   {
      switch (value & ExprValue::TAG_MASK)
      {
         case ExprValue::BOOLEAN:
            return getBool() == other.getBool();
         case ExprValue::NUMBER:
            return getNumber() == other.getNumber();
         case ExprValue::STRING:
            return strcasecmp(getString(), other.getString()) == 0;
         case ExprValue::OBJECT:
            return getObject() == other.getObject();
         default:
            return false;
      }
   }
   else
   {
      const float64_t v1 = getNumber();
      const float64_t v2 = other.getNumber();
      return (std::isnan(v1) || std::isnan(v2)) ? false : (v1 == v2);
   }
}

inline ExprState::ExprState() : mDepth(0), mStringTable(NULL)
{
}

inline ExprState::~ExprState()
{
   clear();
}

inline ExprObject* ExprState::addObject(ExprObject* obj)
{
   mObjects.push_back(obj);
   return obj;
}

inline ExprValue ExprState::callFunction(const char* name, int argc, ExprValue* args)
{
   auto itr = smFunctions.find(name);
   if (itr == smFunctions.end())
   {
      throw std::runtime_error("Invalid function");
   }
   return itr->second.mPtr(this, argc, args);
}

inline ExprValue ExprState::evaluate(ExprNode* root)
{
   return root->evaluate(*this);
}

inline void ExprState::setContext(const char* name, ExprObject* obj)
{
   mContexts[name] = obj;
}

inline ExprObject* ExprState::getContext(const char* name)
{
   auto itr = mContexts.find(name);
   if (itr == mContexts.end())
   {
      throw std::runtime_error("Couldn't find context %s");
   }
   return itr->second;
}

inline void ExprState::clear()
{
   for (ExprObject* obj : mObjects)
   {
      delete obj;
   }
   mObjects.clear();
}

std::string ExprState::substituteExpressions(const std::string &input, int8_t depth)
{
    std::string output;
    size_t pos = 0;

    while (pos < input.length())
    {
        size_t start = input.find("${{", pos);
        if (start == std::string::npos)
        {
            output += input.substr(pos);
            break;
        }

        output += input.substr(pos, start - pos);

        size_t end = input.find("}}", start);
        if (end == std::string::npos)
        {
            output += input.substr(start);
            break;
        }

        std::string expression = input.substr(start + 3, end - (start + 3));
        const char* value = evaluateString(expression).getStringSafe();
       
        if (strstr(value, "${{") != NULL)
        {
           if (depth == 0)
           {
              throw std::runtime_error("Max expression stack depth reached");
           }
           output += substituteExpressions(value, depth--);
        }
        else
        {
           output += value;
        }

        pos = end + 2;
    }

    return output;
}

ExprValue ExprState::substituteSingleExpression(const std::string &input, int8_t depth)
{
   std::string output;
   size_t pos = 0;
   
   size_t start = input.find("${{", pos);
   if (start != std::string::npos)
   {
      size_t end = input.find("}}", start);
      if (end != std::string::npos)
      {
         std::string expression = input.substr(start + 3, end - (start + 3));
         return evaluateString(expression);
      }
   }
   
   return evaluateString(input);
}

ExprValue ExprState::evaluateString(const std::string &input)
{
   if (mDepth > MaxStackDepth)
   {
      throw std::runtime_error("Stack overflow");
   }
   mDepth++;
   printf("DEBUG: evaluating string %s\n", input.c_str());

   // Tokenize
   std::vector<Token> tokens;
   tokenize(input, tokens);
   // Parse
   Parser parser(tokens);
   parser.mStringTable = mStringTable;
   // Compile
   CompiledStatement* stmts = parser.compile();
   if (stmts == NULL)
   {
      return ExprValue();
   }
   
   // Evaluate
   ExprValue result = evaluate(stmts->mRoot);
   delete stmts;
   mDepth--;

   return result;
}

inline ExprObject::ExprObject(ExprState* state) : mState(state), mAddObjectFunc(NULL)
{
   if (state) 
   {
      state->addObject(this);
   }
}

inline ExprObject::~ExprObject()
{
}

inline ExprObject* ExprObject::constructObject()
{
   return mAddObjectFunc ? mAddObjectFunc(mState) : NULL;
}


ExprArray::ExprArray(ExprState* state) : ExprObject(state)
{
}

inline void  ExprArray::clear()
{
   mItems.clear();
}

inline void ExprArray::addArrayValue(ExprValue value)
{
   mItems.push_back(value);
}

inline ExprValue ExprArray::getArrayIndex(uint32_t index)
{
  return mItems[index];
}

inline ExprValue ExprArray::getMapKey(std::string key)
{
  return ExprValue();
}

inline ExprValue ExprArray::setMapKey(std::string key, ExprValue value)
{
   return value;
}

inline void ExprArray::toList(std::vector<ExprValue>& outItems)
{
  outItems = mItems;
}

inline void ExprArray::extractKeys(std::vector<std::string>& outItems)
{
   outItems.clear();
}

inline void ExprArray::getExpandRoots(std::vector<ExprObject*>& outItems)
{
  for (ExprValue& value : mItems)
  {
     ExprObject* obj = value.getObject();
     if (obj)
     {
        outItems.push_back(obj);
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


ExprMap::ExprMap(ExprState* state) : ExprObject(state)
{

}

inline void  ExprMap::clear()
{
   mItems.clear();
}

inline void ExprMap::addArrayValue(ExprValue value)
{
}

inline ExprValue ExprMap::getArrayIndex(uint32_t index)
{
   return ExprValue();
}

inline ExprValue ExprMap::getMapKey(std::string key)
{
   auto itr = mItems.find(key);
   if (itr == mItems.end())
   {
      return ExprValue();
   }
   else
   {
      return mItems[key];
   }
}

inline ExprValue ExprMap::setMapKey(std::string key, ExprValue value)
{
   mItems[key] = value;
   return value;
}

inline void ExprMap::toList(std::vector<ExprValue>& outItems)
{
  outItems.clear();
}

inline void ExprMap::extractKeys(std::vector<std::string>& outItems)
{
   outItems.clear();
   for (const auto& itr : mItems)
   {
      outItems.push_back(itr.first);
   }
}

inline void ExprMap::getExpandRoots(std::vector<ExprObject*>& outItems)
{
  for (auto& itr : mItems)
  {
     ExprObject* obj = itr.second.getObject();
     if (obj)
     {
        outItems.push_back(obj);
     }
  }
}

inline std::string ExprMap::toString()
{
   return "";
}


ExprMultiKey::ExprMultiKey(ExprState* state) : ExprObject(state)
{
   mSlots[0] = NULL;
   mSlots[1] = NULL;
   mSlots[2] = NULL;
}

inline ExprMultiKey::~ExprMultiKey()
{
}

inline void ExprMultiKey::clear()
{
}

inline ExprObject* ExprMultiKey::constructObject()
{
   return NULL;
}

inline void ExprMultiKey::addArrayValue(ExprValue value)
{
}

inline ExprValue ExprMultiKey::getArrayIndex(uint32_t index)
{
   return ExprValue();
}

inline ExprValue ExprMultiKey::getMapKey(std::string key)
{
   for (int i=2; i>=0; i--)
   {
      if (mSlots[i] == NULL)
      {
         continue;
      }
      ExprValue value = mSlots[i]->getMapKey(key);
      if (value.isObject() && value.getObject() == NULL)
      {
         continue;
      }
      return value;
   }
}

inline ExprValue ExprMultiKey::setMapKey(std::string key, ExprValue value)
{
   return ExprValue();
}

inline void ExprMultiKey::toList(std::vector<ExprValue>& outItems)
{
   return ExprValue();
}

inline void ExprMultiKey::extractKeys(std::vector<std::string>& outKeys)
{
   std::set<std::string> keyList;
   std::vector<std::string> newKeys;
   
   for (int i=2; i>=0; i--)
   {
      if (mSlots[i] == NULL)
      {
         continue;
      }
      
      mSlots[i]->extractKeys(newKeys);
      
      for (std::string& key : newKeys)
      {
         keyList.insert(key);
      }
   }
   
   newKeys.clear();
   for (const std::string& itr : keyList)
   {
      newKeys.push_back(itr);
   }
   
   return newKeys;
}

inline std::string ExprMultiKey::toString()
{
   return "";
}

inline void ExprMultiKey::getExpandRoots(std::vector<ExprObject*>& outItems)
{
   outItems.clear();
}


ExprFieldObject::ExprFieldObject(ExprState* state) : ExprObject(state)
{

}

inline void  ExprFieldObject::clear()
{
}

inline void ExprFieldObject::addArrayValue(ExprValue value)
{
}

inline ExprValue ExprFieldObject::getArrayIndex(uint32_t index)
{
   return ExprValue();
}

inline ExprValue ExprFieldObject::getMapKey(std::string key)
{
   auto& reg = getObjectFieldRegistry();
   auto itr = reg.find(key);
   if (itr != reg.end())
   {
      ExprValue* val = (ExprValue*)(((uint8_t*)this) + itr->second.offset);
      return *val;
   }
   return ExprValue();
}

inline ExprValue ExprFieldObject::setMapKey(std::string key, ExprValue value)
{
   auto& reg = getObjectFieldRegistry();
   auto itr = reg.find(key);
   if (itr != reg.end() && 
       ((itr->second.typeMask & value.value) == (value.value & ~ExprValue::PAYLOAD_MASK)) &&
       itr->second.canSet)
   {
      ExprValue* val = (ExprValue*)(((uint8_t*)this) + itr->second.offset);
      *val = value;
   }
   return value;
}

inline void ExprFieldObject::toList(std::vector<ExprValue>& outItems)
{
  outItems.clear();
}

inline void ExprFieldObject::extractKeys(std::vector<std::string>& outItems)
{
   outItems.clear();
}

inline void ExprFieldObject::getExpandRoots(std::vector<ExprObject*>& outItems)
{
}

inline std::string ExprFieldObject::toString()
{
   return "";
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
   ExprValue leftVal = mLeft ? mLeft->evaluate(state) : ExprValue();
   ExprValue rightVal = mRight ? mRight->evaluate(state) : ExprValue();

   switch (mOp)
   {
       case OperatorType::OP_NE:
           return ExprValue().setBool(!leftVal.testEq(rightVal));

       case OperatorType::OP_EQ:
           return ExprValue().setBool(leftVal.testEq(rightVal));

       case OperatorType::OP_LTE:
           return ExprValue().setBool(leftVal.getNumber() <= rightVal.getNumber());

       case OperatorType::OP_GTE:
           return ExprValue().setBool(leftVal.getNumber() >= rightVal.getNumber());

       case OperatorType::OP_GT:
           return ExprValue().setBool(leftVal.getNumber() > rightVal.getNumber());

       case OperatorType::OP_LT:
           return ExprValue().setBool(leftVal.getNumber() < rightVal.getNumber());

       case OperatorType::OP_AND:
           return leftVal.getNumber() ? rightVal : leftVal;

       case OperatorType::OP_OR:
           return leftVal.getNumber() ? leftVal : rightVal;

       case OperatorType::OP_NOT:
           return ExprValue().setBool(!leftVal.getNumber());

       case OperatorType::OP_MUL:
           throw std::runtime_error("Cannot multiply numbers");

       default:
           break;
   }

   return ExprValue();
}

inline KeyAccessNode::KeyAccessNode(ExprNode* object, std::string key)
: mObject(object), mKey(std::move(key))
{
}

inline ExprValue KeyAccessNode::evaluate(ExprState& state)
{
   ExprObject* mapObject = mObject->evaluate(state).getObject();
   if (mapObject == NULL)
   {
      throw std::runtime_error("Map object is not present");
   }
   return mapObject->getMapKey(mKey);
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
   ExprObject* baseObject = mLeft->evaluate(state).getObject();
   
   std::vector<ExprObject*> expandRoots;
   if (baseObject == NULL)
   {
      throw std::runtime_error("Not an object");
   }
   
   ExprArray* array = new ExprArray(&state);
   ExprValue retVal;
   retVal.setObject(array);
   
   baseObject->getExpandRoots(expandRoots);
   ExprValue expandKey = mRight->evaluate(state);
   for (ExprObject* obj : expandRoots)
   {
      ExprValue value = obj->getMapKey(expandKey.getString());
      if (!value.isNull())
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
   if (!index.isNumber())
   {
      throw std::runtime_error("Array accessor is not numeric");
   }
   ExprObject* arrayObject = mArray->evaluate(state).getObject();
   if (arrayObject == NULL)
   {
      throw std::runtime_error("Array object is not present");
   }
   return arrayObject->getArrayIndex((uint32_t)index.getNumber());
}

inline IdentifierNode::IdentifierNode(std::string name) : mName(std::move(name))
{
}

inline ExprValue IdentifierNode::evaluate(ExprState& state)
{
   ExprValue val;
   val.setObject(state.getContext(mName.c_str()));
   return val;
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
      value.setString(*mStringTable, token.value.c_str());
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
         
         primary = allocNode<ExpandoNode>(primary, new LiteralNode(ExprValue().setString(*mStringTable, rightKey.c_str())));
      }
      else
      {
         break;
      }
   }
   
   return primary;
}

}
