#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "matrix.h"
#include "semiring.h"
#include "var.h"

#include "free-structure.h"

template <typename SR>
class Evaluator;

class FreeSemiring : public Semiring<FreeSemiring> {
  public:
    /* Default constructor creates zero element. */
    FreeSemiring() {
      InitFactory();
      assert(factory_);
      node_ = factory_->GetEmpty();
    }

    FreeSemiring(const VarPtr var) {
      assert(var);
      InitFactory();
      assert(factory_);
      node_ = factory_->NewElement(var);
    }

    static FreeSemiring null() {
      InitFactory();
      assert(factory_);
      return FreeSemiring{factory_->GetEmpty()};
    }

    static FreeSemiring one() {
      InitFactory();
      assert(factory_);
      return FreeSemiring{factory_->GetEpsilon()};
    }

    FreeSemiring star() const {
      assert(factory_);
      return FreeSemiring{factory_->NewStar(node_)};
    }

    FreeSemiring operator+(const FreeSemiring &x) {
      assert(factory_);
      return FreeSemiring{factory_->NewAddition(node_, x.node_)};
    }

    FreeSemiring& operator+=(const FreeSemiring &x) {
      assert(factory_);
      node_ = factory_->NewAddition(node_, x.node_);
      return *this;
    }

    FreeSemiring operator*(const FreeSemiring &x) {
      assert(factory_);
      return FreeSemiring{factory_->NewMultiplication(node_, x.node_)};
    }

    FreeSemiring& operator*=(const FreeSemiring &x) {
      assert(factory_);
      node_ = factory_->NewMultiplication(node_, x.node_);
      return *this;
    }

    bool operator==(const FreeSemiring &x) const {
      return node_ == x.node_;
    }

    std::string string() const {
      std::stringstream ss;
      ss << *node_;
      return ss.str();
    }

    template <typename SR>
    SR Eval(const std::unordered_map<VarPtr, SR> &valuation) const;

    template <typename SR>
    SR Eval(Evaluator<SR> &evaluator) const;

    void PrintDot(std::ostream &out) {
      assert(factory_);
      factory_->PrintDot(out);
    }

    void GC() {
      assert(factory_);
      factory_->GC();
    }

  private:
    FreeSemiring(NodePtr n) : node_(n) { assert(factory_); }

    static void InitFactory() {
      if (factory_ == nullptr) {
        factory_ = std::move(std::unique_ptr<NodeFactory>(new NodeFactory));
      }
    }

    NodePtr node_;
    static std::unique_ptr<NodeFactory> factory_;

    friend struct std::hash<FreeSemiring>;
};

namespace std {

template <>
struct hash<FreeSemiring> {
  inline std::size_t operator()(const FreeSemiring &fs) const {
    std::hash<NodePtr> h;
    return h(fs.node_);
  }
};

}  /* namespace std */


/*
 * Evaluator
 *
 * We want to memoize the result of evaluating every subgraph, since we can
 * reach the same node (and thus the same subgraph) many times.  So only the
 * first one we will actually perform the computation, in all subsequent visits
 * we will reuse the memoized reslt.  Note that this is ok, because we never
 * modify the actual semiring values.  We use shared_ptrs to avoid any
 * unnecessary copying of temporary values, etc.
 */
template <typename SR>
class Evaluator : public NodeVisitor {
  typedef std::shared_ptr<SR> SRPtr_;
  public:
    Evaluator(const std::unordered_map<VarPtr, SR> &v)
        : val_(v), evaled_(), result_() {}

    void Visit(const Addition &a) {
      LookupEval(a.GetLhs());
      auto temp = std::move(result_);
      LookupEval(a.GetRhs());
      result_ = std::make_shared<SR>(*temp + *result_);
    }

    void Visit(const Multiplication &m) {
      LookupEval(m.GetLhs());
      auto temp = std::move(result_);
      LookupEval(m.GetRhs());
      result_ = std::make_shared<SR>(*temp * *result_);
    }

    void Visit(const Star &s) {
      LookupEval(s.GetNode());
      result_ = std::make_shared<SR>(result_->star());
    }

    void Visit(const Element &e) {
      auto iter = val_.find(e.GetVar());
      assert(iter != val_.end());
      result_ = std::make_shared<SR>(iter->second);
    }

    void Visit(const Epsilon &e) {
      result_ = std::make_shared<SR>(SR::one());
    }

    void Visit(const Empty &e) {
      result_ = std::make_shared<SR>(SR::null());
    }

    std::shared_ptr<SR> GetResult() {
      return result_;
    }

    static const bool is_idempotent = false;
    static const bool is_commutative = false;

  private:
    void LookupEval(const NodePtr &node) {
      auto iter = evaled_.find(node);
      if (iter != evaled_.end()) {
        result_ = iter->second;
      } else {
        node->Accept(*this);  /* Sets the result_ correctly */
        evaled_.emplace(node, result_);
      }
    }

    const std::unordered_map<VarPtr, SR> &val_;
    std::unordered_map<NodePtr, SRPtr_> evaled_;
    SRPtr_ result_;
};

template <typename SR>
SR FreeSemiring::Eval(const std::unordered_map<VarPtr, SR> &valuation) const {
  Evaluator<SR> evaluator{valuation};
  node_->Accept(evaluator);
  return std::move(*evaluator.GetResult());
}

template <typename SR>
SR FreeSemiring::Eval(Evaluator<SR> &evaluator) const {
  node_->Accept(evaluator);
  return std::move(*evaluator.GetResult());
}

template <typename SR>
Matrix<SR> FreeSemiringMatrixEval(const Matrix<FreeSemiring> &matrix,
    const std::unordered_map<VarPtr, SR> &valuation) {

  std::vector<FreeSemiring> elements = matrix.getElements();
  std::vector<SR> result;

  /* We have a single Evaluator that is used for all evaluations of the elements
   * of the original matrix.  This way if different elements refer to the same
   * FreeSemiring subexpression, we memoize the result and reuse it. */
  Evaluator<SR> evaluator{valuation};

  for(auto &elem : elements) {
    result.emplace_back(elem.Eval(evaluator));
  }

  return Matrix<SR>(matrix.getRows(), matrix.getColumns(), std::move(result));
}

/* FIXME: Temporary wrapper for compatibility with the old implementation. */
template <typename SR>
SR FreeSemiring_eval(FreeSemiring elem,
    std::unordered_map<VarPtr, SR> *valuation) {
  return elem.Eval(*valuation);
}

/* FIXME: Temporary wrapper for compatibility with the old implementation. */
template <typename SR>
Matrix<SR> FreeSemiring_eval(Matrix<FreeSemiring> matrix,
    std::unordered_map<VarPtr, SR> *valuation) {
  return FreeSemiringMatrixEval(matrix, *valuation);
}
