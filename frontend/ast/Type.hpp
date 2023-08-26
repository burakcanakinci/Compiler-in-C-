#ifndef TYPE_HPP
#define TYPE_HPP

#include <cassert>
#include <string>
#include <vector>

class Type {
public:
  /// Basic type variants. Numerical ones are ordered by conversion rank.
  enum VariantKind {
    Invalid,
    Composite,
    Void,
    Char,
    UnsignedChar,
    Short,
    UnsignedShort,
    Int,
    UnsignedInt,
    Long,
    UnsignedLong,
    LongLong,
    UnsignedLongLong,
    Float,
    Double,
  };
  // TODO: investigate if Simple effectively means scalar, if so rename it
  // to Scalar for clarity
  enum TypeKind { Simple, Array, Struct };
  enum TypeQualifier : unsigned { None, Typedef, Const };

  std::string GetName() const { return Name; }
  void SetName(std::string &n) { Name = n; }

  void SetTypeKind(TypeKind t) { Kind = t; }

  VariantKind GetTypeVariant() const { return Ty; }
  void SetTypeVariant(VariantKind t) { Ty = t; }

  void SetQualifiers(unsigned q) { Qualifiers = q; }

  uint8_t GetPointerLevel() const { return PointerLevel; }
  void IncrementPointerLevel() { PointerLevel++; }
  void DecrementPointerLevel() {
    if (PointerLevel > 0)
      PointerLevel--;
  }

  bool IsPointerType() const { return PointerLevel != 0; }

  bool HasVarArg() const { return VarArg; }
  void SetVarArg(bool p) { VarArg = p; }

  static std::string ToString(const Type *t);

  /// Given two type variants it return the stronger one.
  /// Type variants must be numerical ones.
  /// Example Int and Double -> result Double.
  static Type GetStrongestType(const Type &type1, const Type &type2) {
    if (type1.GetTypeVariant() > type2.GetTypeVariant())
      return type1;
    else
      return type2;
  }

  static bool IsImplicitlyCastable(const Type::VariantKind from,
                                   const Type::VariantKind to) {
    switch (to) {
    case Char:
    case UnsignedChar:
    case Short:
    case UnsignedShort:
    case Int:
    case UnsignedInt:
    case Long:
    case UnsignedLong:
    case LongLong:
    case UnsignedLongLong:
      return from >= Char;
    default:
      return false;
    }
  }

  static bool IsImplicitlyCastable(const Type &from, const Type &to) {
    const bool IsToPtr = to.IsPointerType();
    const bool IsFromPtr = from.IsPointerType();
    const bool IsFromArray = from.IsArray();

    // function assignment case
    if ((from.IsFunction() && !to.IsPointerType()) ||
        (to.IsFunction() && !from.IsPointerType()))
      return false;

    // array to pointer decay case
    if (IsFromArray && !IsFromPtr && IsToPtr) {
      if (from.GetTypeVariant() == to.GetTypeVariant())
        return true;
      return false;
    }

    // from integer type to pointer
    if (IsToPtr && from.IsIntegerType())
      return true;

    bool Result;
    switch (to.GetTypeVariant()) {
    case Char:
    case UnsignedChar:
    case Short:
    case UnsignedShort:
    case Int:
    case UnsignedInt:
    case Long:
    case UnsignedLong:
    case LongLong:
    case UnsignedLongLong:
    case Double:
      Result = from.GetTypeVariant() >= Char;
      break;
    default:
      return false;
    }

    return Result;
  }

  static bool IsSmallerThanInt(const Type::VariantKind v) {
    switch (v) {
    case Char:
    case UnsignedChar:
    case Short:
    case UnsignedShort:
      return true;
    default:
      return false;
    }
  }

  static bool OnlySigndnessDifference(const Type::VariantKind v1,
                                      const Type::VariantKind v2) {
    if ((v1 == Int && v2 == UnsignedInt) || (v2 == Int && v1 == UnsignedInt) ||
        (v1 == Char && v2 == UnsignedChar) ||
        (v2 == Char && v1 == UnsignedChar) ||
        (v1 == Short && v2 == UnsignedShort) ||
        (v2 == Short && v1 == UnsignedShort) ||
        ((v1 == Long || v1 == LongLong) &&
         (v2 == UnsignedLong || v2 == UnsignedLongLong)) ||
        ((v2 == Long || v2 == LongLong) &&
         (v1 == UnsignedLong || v1 == UnsignedLongLong)))
      return true;

    // special case, not really sign difference, rather just same size
    if ((v1 == Long && v2 == LongLong) || (v2 == Long && v1 == LongLong))
      return true;

    return false;
  }

  Type() : Kind(Simple), Ty(Invalid) {}
  explicit Type(TypeKind tk) {
    Kind = tk;
    switch (tk) {
    case Array:
    case Struct:
      Ty = Composite;
      break;
    case Simple:
    default:
      Ty = Invalid;
      break;
    }
  }
  explicit Type(VariantKind vk) {
    Kind = Simple;
    Ty = vk;
  }

  Type(const Type &t, std::vector<unsigned> d) : Type(t) {
    if (d.empty()) {
      Kind = t.Kind;
    } else {
      Kind = Array;
      Dimensions = std::move(d);
    }
  }

  Type(const Type &t, std::vector<Type> a) {
    ParameterList = std::move(a);
    Ty = t.GetTypeVariant();
    Kind = Simple;
  }

  Type(Type &&ct) = default;
  Type(const Type &ct) = default;
  Type &operator=(const Type &ct) = default;

  bool IsArray() const { return Kind == Array; }
  bool IsFunction() const { return !ParameterList.empty(); }
  bool IsStruct() const { return Kind == Struct; }
  bool IsIntegerType() const {
    switch (Ty) {
    case Char:
    case UnsignedChar:
    case Short:
    case UnsignedShort:
    case Int:
    case UnsignedInt:
    case Long:
    case UnsignedLong:
    case LongLong:
    case UnsignedLongLong:
      return true;
    default:
      return false;
    }
  }

  bool IsUnsigned() const {
    switch (Ty) {
    case UnsignedChar:
    case UnsignedShort:
    case UnsignedInt:
    case UnsignedLong:
    case UnsignedLongLong:
      return true;
    default:
      return false;
    }
  }

  bool IsFloatingPoint() const { return Ty == Float || Ty == Double; }
  bool IsVoid() const { return Ty == Void && PointerLevel == 0; }
  bool IsConst() const { return Qualifiers & Const; }
  bool IsTypedef() const { return Qualifiers & Typedef; }

  std::vector<Type> &GetTypeList() { return TypeList; }
  std::vector<Type> &GetParameterList() { return ParameterList; }
  VariantKind GetReturnType() const { return Ty; }

  std::vector<unsigned> &GetDimensions() {
    assert(IsArray() && "Must be an Array type to access Dimensions.");
    return Dimensions;
  }

  void SetDimensions(std::vector<unsigned> D) {
    Kind = Array;
    Dimensions = std::move(D);
  }

  void RemoveFirstDimension() {
    assert(IsArray() && "Must be an Array type to access Dimensions.");
    assert(!Dimensions.empty());
    Dimensions.erase(Dimensions.begin());
    if (Dimensions.empty())
      Kind = Simple; // TODO: what if its an array of struct objects?
  }

  std::vector<Type> &GetArgTypes() { return ParameterList; }

  friend bool operator==(const Type &lhs, const Type &rhs) {
    bool result = lhs.Kind == rhs.Kind && lhs.Ty == rhs.Ty;
    result &= lhs.GetPointerLevel() == rhs.GetPointerLevel();

    if (!result)
      return false;

    if (lhs.ParameterList.size() != rhs.ParameterList.size())
      return false;

    // at this point both type's parameter list has the same size
    if (!lhs.ParameterList.empty()) {
      for (size_t i = 0; i < lhs.ParameterList.size(); i++)
        if (lhs.ParameterList[i] != rhs.ParameterList[i])
          return false;
    }

    switch (lhs.Kind) {
    case Array:
      if (lhs.Dimensions.size() != rhs.Dimensions.size())
        return false;
      else
        for (size_t i = 0; i < lhs.Dimensions.size(); i++)
          if (lhs.Dimensions[i] != rhs.Dimensions[i])
            return false;
      break;
    default:
      break;
    }
    return true;
  }

  friend bool operator!=(const Type &lhs, const Type &rhs) {
    return !(lhs == rhs);
  }

  std::string ToString() const;

private:
  std::string Name; // For structs
  VariantKind Ty;
  uint8_t PointerLevel = 0;

  TypeKind Kind;
  unsigned Qualifiers = None;
  // TODO: revisit the use of union, deleted from here since
  // it just made things complicated
  std::vector<Type> TypeList;
  std::vector<Type> ParameterList;
  std::vector<unsigned> Dimensions;

  /// To indicate whether the function type has variable arguments or not
  bool VarArg = false;
};

// Hold an integer or a float value
class ValueType {
public:
  ValueType() : Kind(Empty) {}
  explicit ValueType(unsigned v) : Kind(Integer), IntVal(v) {}
  explicit ValueType(double v) : Kind(Float), FloatVal(v) {}

  ValueType(ValueType &&) = default;
  ValueType &operator=(ValueType &&) = delete;

  ValueType(const ValueType &) = default;
  ValueType &operator=(const ValueType &) = delete;

  bool IsInt() { return Kind == Integer; }
  bool IsFloat() { return Kind == Float; }
  bool IsEmpty() { return Kind == Empty; }

  int GetIntVal() {
    assert(IsInt() && "Must be an integer type.");
    return IntVal;
  }
  double GetFloatVal() {
    assert(IsFloat() && "Must be a float type.");
    return FloatVal;
  }

  friend bool operator==(const ValueType &lhs, const ValueType &rhs) {
    bool result = lhs.Kind == rhs.Kind;

    if (!result)
      return false;

    switch (lhs.Kind) {
    case Integer:
      result = lhs.IntVal == rhs.IntVal;
      break;
    case Float:
      result = lhs.FloatVal == rhs.FloatVal;
      break;
    default:
      break;
    }
    return result;
  }

private:
  enum ValueKind { Empty, Integer, Float };
  ValueKind Kind;
  union {
    unsigned IntVal{};
    double FloatVal;
  };
};

#endif