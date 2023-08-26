#include "Lexer.hpp"
#include <cassert>
#include <cctype>

std::unordered_map<std::string, Token::TokenKind> Lexer::Keywords =
    std::unordered_map<std::string, Token::TokenKind>{
        {"const", Token::Const},
        {"int", Token::Int},
        {"short", Token::Short},
        {"long", Token::Long},
        {"float", Token::Float},
        {"double", Token::Double},
        {"unsigned", Token::Unsigned},
        {"signed", Token::Signed},
        {"void", Token::Void},
        {"char", Token::Char},
        {"if", Token::If},
        {"switch", Token::Switch},
        {"case", Token::Case},
        {"default", Token::Default},
        {"break", Token::Break},
        {"else", Token::Else},
        {"for", Token::For},
        {"while", Token::While},
        {"return", Token::Return},
        {"do", Token::Do},
        {"struct", Token::Struct},
        {"sizeof", Token::Sizeof},
        {"enum", Token::Enum},
        {"typedef", Token::Typedef},
        {"continue", Token::Continue},
        {"_Bool", Token::Bool},
        {"_Alignas", Token::Alignas},
        {"_Alignof", Token::Alignof},
        {"_Atomic", Token::Atomic},
        {"_Complex", Token::Complex},
        {"_Generic", Token::Generic},
        {"_Imaginary", Token::Imaginary},
        {"_Noreturn", Token::Noreturn},
        {"_Static_assert", Token::StaticAssert},
        {"_Thread_local", Token::ThreadLocal},
    };

Lexer::Lexer(std::vector<std::string> &s) {
  Source = std::move(s);
  TokenBuffer = std::vector<Token>();
  LineIndex = 0;
  ColumnIndex = 0;

  LookAhead(1);
}

void Lexer::ConsumeCurrentToken() {
  assert(!TokenBuffer.empty() && "TokenBuffer is empty.");
  TokenBuffer.erase(TokenBuffer.begin());
}

int Lexer::GetNextChar() {
  // If it is an empty line then move forward by calling EatNextChar() which
  // will advance to the next line's first character.
  if (LineIndex < Source.size() && Source[LineIndex].length() == 0)
    EatNextChar();
  if (LineIndex >= Source.size() || (LineIndex == Source.size() - 1 &&
                                     ColumnIndex == Source[LineIndex].length()))
    return EOF;
  return Source[LineIndex][ColumnIndex];
}

int Lexer::GetNextNthCharOnSameLine(unsigned n) {
  if (LineIndex >= Source.size() ||
      (ColumnIndex + n >= Source[LineIndex].length()))
    return EOF;
  return Source[LineIndex][ColumnIndex + n];
}

void Lexer::EatNextChar() {
  if (LineIndex < Source.size()) {
    if (Source[LineIndex].empty() ||
        ColumnIndex >= Source[LineIndex].size() - 1) {
      ColumnIndex = 0;
      LineIndex++;
    } else {
      ColumnIndex++;
    }
  }
}

std::optional<Token> Lexer::LexNumber() {
  unsigned StartLineIndex = LineIndex;
  unsigned StartColumnIndex = ColumnIndex;
  unsigned Length = 0;
  auto TokenKind = Token::Integer;
  unsigned TokenValue = 0;

  while (isdigit(GetNextChar())) {
    Length++;
    EatNextChar();
  }

  // hex literal case
  if (Length == 1 && GetNextChar() == 'x') {
    Length++;
    EatNextChar();
    uint64_t value = 0;

    int c = GetNextChar();
    while (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
      Length++;
      EatNextChar();
      unsigned currDigit;

      if (isdigit(c))
        currDigit = c - '0';
      else if (islower(c))
        currDigit = c - 'a' + 10;
      else
        currDigit = c - 'A' + 10;

      value = (value << 4) + currDigit;

      c = GetNextChar();
    }

    TokenValue = value;
  }
  // if it is a real value like 3.14
  else if (GetNextChar() == '.') {
    Length++;
    EatNextChar();
    TokenKind = Token::Real;

    if (!isdigit(GetNextChar()))
      return std::nullopt; // TODO it might be better to make Invalid token

    while (isdigit(GetNextChar())) {
      Length++;
      EatNextChar();
    }
  }

  if (Length == 0)
    return std::nullopt;

  std::string_view StringValue{&Source[StartLineIndex][StartColumnIndex],
                               Length};
  return Token(TokenKind, StringValue, StartLineIndex, StartColumnIndex,
               TokenValue);
}

std::optional<Token> Lexer::LexIdentifier() {
  unsigned StartLineIndex = LineIndex;
  unsigned StartColumnIndex = ColumnIndex;
  unsigned Length = 0;

  // Cannot start with a digit
  if (isdigit(GetNextChar()))
    return std::nullopt;

  while (isalnum(GetNextChar()) || GetNextChar() == '_') {
    Length++;
    EatNextChar();
  }

  if (Length == 0)
    return std::nullopt;

  std::string_view StringValue{&Source[StartLineIndex][StartColumnIndex],
                               Length};
  return Token(Token::Identifier, StringValue, StartLineIndex,
               StartColumnIndex);
}

std::optional<Token> Lexer::LexKeyword() {
  std::size_t WordEnd = Source[LineIndex]
                            .substr(ColumnIndex)
                            .find_first_of("\t\n\v\f\r;(){}[]:* ");

  auto Word = Source[LineIndex].substr(ColumnIndex, WordEnd);

  if (!Keywords.count(Word))
    return std::nullopt;

  unsigned StartLineIndex = LineIndex;
  unsigned StartColumnIndex = ColumnIndex;

  for (int i = Word.length(); i > 0; i--)
    EatNextChar();

  std::string_view StringValue{&Source[StartLineIndex][StartColumnIndex],
                               Word.length()};
  return Token(Lexer::Keywords[Word], StringValue, StartLineIndex,
               StartColumnIndex);
}

std::optional<Token> Lexer::LexCharLiteral() {
  unsigned StartLineIndex = LineIndex;
  unsigned StartColumnIndex = ColumnIndex;

  // It must start with a ' char
  if (GetNextChar() != '\'')
    return std::nullopt;

  EatNextChar(); // eat ' char

  bool isEscaped = false;
  if (GetNextChar() == '\\') {
    isEscaped = true;
    EatNextChar();
  }

  char currentChar = GetNextChar();
  EatNextChar();
  unsigned value = -1; // to signal errors

  // TODO: add support for other cases like multiple octal digits, hexa etc
  if (isEscaped) {
    if (isdigit(currentChar)) {
      assert(currentChar < '8' && "Expecting octal digits");
      value = currentChar - '0';
    } else if (currentChar == 'a')
      value = 0x07;
    else if (currentChar == 'b')
      value = 0x08;
    else if (currentChar == 'e')
      value = 0x1B;
    else if (currentChar == 'f')
      value = 0x0C;
    else if (currentChar == 'n')
      value = 0x0A;
    else if (currentChar == 'r')
      value = 0x0D;
    else if (currentChar == 't')
      value = 0x0B;
    else
      assert("TODO: add the other ones");
  } else {
    value = currentChar;
  }

  if (GetNextChar() != '\'')
    return Token(Token::Invalid);

  EatNextChar(); // eat ending ' char

  std::string_view StringValue{&Source[StartLineIndex][StartColumnIndex],
                               ColumnIndex - StartColumnIndex + 1};
  return Token(Token::CharacterLiteral, StringValue, StartLineIndex,
               StartColumnIndex, value);
}

std::optional<Token> Lexer::LexStringLiteral() {
  unsigned StartLineIndex = LineIndex;
  unsigned StartColumnIndex = ColumnIndex;
  unsigned Length = 0;

  // It must start with a " char
  if (GetNextChar() != '"')
    return std::nullopt;

  EatNextChar(); // eat " char
  Length++;

  bool lastCharIsEscape = false; // used to determine if encountered a '\"'

  while ((GetNextChar() != '"' || (GetNextChar() == '"' && lastCharIsEscape)) &&
         GetNextChar() != EOF) {
    char c = GetNextChar();
    lastCharIsEscape = c == '\\';
    EatNextChar();
    Length++;
  }

  if (GetNextChar() != '"')
    return Token(Token::Invalid);

  EatNextChar(); // eat " char
  Length++;

  std::string_view StringValue{&Source[StartLineIndex][StartColumnIndex],
                               Length};
  return Token(Token::StringLiteral, StringValue, StartLineIndex,
               StartColumnIndex);
}

std::optional<Token> Lexer::LexSymbol() {
  Token::TokenKind TokenKind;
  unsigned Size = 1;

  switch (GetNextChar()) {
  case '.':
    if (GetNextNthCharOnSameLine(1) == '.' &&
        GetNextNthCharOnSameLine(2) == '.') {
      TokenKind = Token::DotDotDot;
      Size = 3;
    } else
      TokenKind = Token::Dot;
    break;
  case ',':
    TokenKind = Token::Comma;
    break;
  case '+':
    if (GetNextNthCharOnSameLine(1) == '+') {
      TokenKind = Token::PlusPlus;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::PlusEqual;
      Size = 2;
    } else
      TokenKind = Token::Plus;
    break;
  case '-':
    if (GetNextNthCharOnSameLine(1) == '-') {
      TokenKind = Token::MinusMinus;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '>') {
      TokenKind = Token::MinusGreaterThan;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::MinusEqual;
      Size = 2;
    } else
      TokenKind = Token::Minus;
    break;
  case '*':
    if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::AstrixEqual;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '/') {
      TokenKind = Token::AstrixForwardSlash;
      Size = 2;
    } else
      TokenKind = Token::Astrix;
    break;
  case '/':
    if (GetNextNthCharOnSameLine(1) == '/') {
      TokenKind = Token::DoubleForwardSlash;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '*') {
      TokenKind = Token::ForwardSlashAstrix;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::ForwardSlashEqual;
      Size = 2;
    } else
      TokenKind = Token::ForwardSlash;
    break;
  case '%':
    if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::PercentEqual;
      Size = 2;
    } else
      TokenKind = Token::Percent;
    break;
  case '=':
    if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::DoubleEqual;
      Size = 2;
    } else
      TokenKind = Token::Equal;
    break;
  case '<':
    if (GetNextNthCharOnSameLine(1) == '<') {
      if (GetNextNthCharOnSameLine(2) == '=') {
        TokenKind = Token::LessThanLessThanEqual;
        Size = 3;
      } else {
        TokenKind = Token::LessThanLessThan;
        Size = 2;
      }
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::LessEqual;
      Size = 2;
    } else
      TokenKind = Token::LessThan;
    break;
  case '>':
    if (GetNextNthCharOnSameLine(1) == '>') {
      if (GetNextNthCharOnSameLine(2) == '=') {
        TokenKind = Token::GreaterThanGreaterThanEqual;
        Size = 3;
      } else {
        TokenKind = Token::GreaterThanGreaterThan;
        Size = 2;
      }
      break;
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::GreaterEqual;
      Size = 2;
    } else
      TokenKind = Token::GreaterThan;
    break;
  case '!':
    if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::BangEqual;
      Size = 2;
    } else
      TokenKind = Token::Bang;
    break;
  case '?':
    TokenKind = Token::QuestionMark;
    break;
  case '&':
    if (GetNextNthCharOnSameLine(1) == '&') {
      TokenKind = Token::DoubleAnd;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::AndEqual;
      Size = 2;
    } else
      TokenKind = Token::And;
    break;
  case '|':
    if (GetNextNthCharOnSameLine(1) == '|') {
      TokenKind = Token::DoubleOr;
      Size = 2;
    } else if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::OrEqual;
      Size = 2;
    } else
      TokenKind = Token::Or;
    break;
  case '^':
    if (GetNextNthCharOnSameLine(1) == '=') {
      TokenKind = Token::CaretEqual;
      Size = 2;
    } else
      TokenKind = Token::Caret;
    break;
  case '~':
    TokenKind = Token::Tilde;
    break;
  case ':':
    TokenKind = Token::Colon;
    break;
  case ';':
    TokenKind = Token::SemiColon;
    break;
  case '(':
    TokenKind = Token::LeftParen;
    break;
  case ')':
    TokenKind = Token::RightParen;
    break;
  case '[':
    TokenKind = Token::LeftBracket;
    break;
  case ']':
    TokenKind = Token::RightBracket;
    break;
  case '{':
    TokenKind = Token::LeftCurly;
    break;
  case '}':
    TokenKind = Token::RightCurly;
    break;
  case '\\':
    TokenKind = Token::BackSlash;
    break;
  default:
    return std::nullopt;
    break;
  }

  std::string_view StringValue{&Source[LineIndex][ColumnIndex], Size};
  auto Result = Token(TokenKind, StringValue, LineIndex, ColumnIndex);

  EatNextChar();
  if (Size >= 2)
    EatNextChar();
  if (Size == 3)
    EatNextChar();

  return Result;
}

Token Lexer::LookAhead(unsigned n) {
  // fill in the TokenBuffer to have at least n element
  for (size_t i = TokenBuffer.size(); i < n; i++)
    TokenBuffer.push_back(Lex(true));

  return TokenBuffer[n - 1];
}

bool Lexer::Is(Token::TokenKind tk) {
  // fill in the buffer with one token if it is empty
  if (TokenBuffer.size() == 0)
    LookAhead(1);

  return GetCurrentToken().GetKind() == tk;
}

bool Lexer::IsNot(Token::TokenKind tk) { return !Is(tk); }

Token Lexer::Lex(bool LookAhead) {
  // if the TokenBuffer not empty then return the Token from there
  // and remove it from the stack
  if (TokenBuffer.size() > 0 && !LookAhead) {
    auto CurrentToken = GetCurrentToken();
    ConsumeCurrentToken();
    return CurrentToken;
  }

  int CurrentCharacter = GetNextChar();
  std::string WhiteSpaceChars("\t\n\v\f\r ");

  // consume white space characters
  while (WhiteSpaceChars.find(CurrentCharacter) != std::string::npos ||
         CurrentCharacter == '\0') {
    EatNextChar();
    CurrentCharacter = GetNextChar();
  }

  if (CurrentCharacter == EOF) {
    return Token(Token::EndOfFile);
  }

  auto Result = LexKeyword();

  if (!Result)
    Result = LexSymbol();
  if (!Result)
    Result = LexNumber();
  if (!Result)
    Result = LexCharLiteral();
  if (!Result)
    Result = LexStringLiteral();
  if (!Result)
    Result = LexIdentifier();

  // Handle single line comment. If "//" detected, then advance to next line and
  // lex again.
  if (Result.has_value() &&
      Result.value().GetKind() == Token::DoubleForwardSlash) {
    LineIndex++;
    ColumnIndex = 0;
    return Lex();
  }

  // Handle multiline comments like /* ... */
  if (Result.has_value() &&
      Result.value().GetKind() == Token::ForwardSlashAstrix) {
    while (GetNextChar() != EOF &&
           (GetNextChar() != '*' || GetNextNthCharOnSameLine(1) != '/')) {
      EatNextChar();
    }
    EatNextChar();
    EatNextChar();

    return Lex();
  }

  if (Result)
    return Result.value();

  return Token(Token::Invalid);
}
