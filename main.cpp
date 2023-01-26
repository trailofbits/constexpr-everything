#include <iostream>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;

namespace {
llvm::cl::OptionCategory ConstexprCategory("constexpr-everything [-fix]");

llvm::cl::extrahelp ConstexprCategoryHelp(R"(
Use clang's existing constexpr validation code to automatically apply constexpr where appropriate
    )");

llvm::cl::opt<bool> ConstExprFixItOption("fix", llvm::cl::init(false), llvm::cl::desc("apply fix-its to existing code"),
                                         llvm::cl::cat(ConstexprCategory));

llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
}  // namespace

namespace {

// These functions are stolen from clang::Sema, where they're private.
// From lib/Sema/SemaDeclCXX.cpp

/// Check that the given type is a literal type. Issue a diagnostic if not,
/// if Kind is Diagnose.
/// \return \c true if a problem has been found (and optionally diagnosed).
template <typename... Ts>
static bool CheckLiteralType(Sema& SemaRef, Sema::CheckConstexprKind Kind, SourceLocation Loc, QualType T, unsigned DiagID, Ts&&... DiagArgs) {
  if (T->isDependentType()) {
    return false;
  }

  switch (Kind) {
    case Sema::CheckConstexprKind::Diagnose:
      return SemaRef.RequireLiteralType(Loc, T, DiagID, std::forward<Ts>(DiagArgs)...);

    case Sema::CheckConstexprKind::CheckValid:
      return !T->isLiteralType(SemaRef.Context);
  }

  llvm_unreachable("unknown CheckConstexprKind");
}

// CheckConstexprParameterTypes - Check whether a function's parameter types
// are all literal types. If so, return true. If not, produce a suitable
// diagnostic and return false.

// from lib/Sema/SemaDeclCXX.cpp
static bool CheckConstexprParameterTypes(Sema& SemaRef, const FunctionDecl* FD) {
  unsigned ArgIndex = 0;
  const auto* FT = FD->getType()->getAs<FunctionProtoType>();
  for (FunctionProtoType::param_type_iterator i = FT->param_type_begin(), e = FT->param_type_end(); i != e; ++i, ++ArgIndex) {
    const ParmVarDecl* PD = FD->getParamDecl(ArgIndex);
    SourceLocation ParamLoc = PD->getLocation();
    if (!(*i)->isDependentType()
        && SemaRef.RequireLiteralType(ParamLoc, *i, diag::err_constexpr_non_literal_param, ArgIndex + 1, PD->getSourceRange(),
                                      isa<CXXConstructorDecl>(FD))) {
      return false;
    }
  }
  return true;
}
}  // namespace

/*
 * ConstexprFunctionASTVisitor
 *
 * Find all functions that can be constexpr but arent. Create diagnostics for
 * them and mark them constexpr for the next pass.
 */
class ConstexprFunctionASTVisitor : public clang::RecursiveASTVisitor<ConstexprFunctionASTVisitor>
{
  clang::SourceManager& sourceManager_;
  clang::CompilerInstance& CI_;
  clang::DiagnosticsEngine& DE;

 public:
  explicit ConstexprFunctionASTVisitor(clang::SourceManager& sm, clang::CompilerInstance& ci)
    : sourceManager_(sm), CI_(ci), DE(ci.getASTContext().getDiagnostics()) {}

  bool VisitFunctionDecl(clang::FunctionDecl* func) {

    // Only functions in our TU
    SourceLocation const loc = func->getSourceRange().getBegin();
    if (!sourceManager_.isWrittenInMainFile(loc)) {
      return true;
    }

    // Skip existing constExpr functions
    if (func->isConstexpr()) {
      return true;
    }

    // Don't mark main as constexpr
    if (func->isMain()) {
      return true;
    }

    // Destructors can't be constexpr
    if (isa<CXXDestructorDecl>(func)) {
      return true;
    }

    auto& sema = CI_.getSema();

    // Temporarily disable diagnostics for these next functions, use a
    // unique_ptr deleter to handle restoring it
    sema.getDiagnostics().setSuppressAllDiagnostics(true);
    {
      auto returnDiagnostics = [&sema](int*) { sema.getDiagnostics().setSuppressAllDiagnostics(false); };
      int lol = 0;
      std::unique_ptr<int, decltype(returnDiagnostics)> const scope(&lol, returnDiagnostics);

#if LLVM_VERSION_MAJOR >= 10
      if (!sema.CheckConstexprFunctionDefinition(func, Sema::CheckConstexprKind::CheckValid)) {
        return true;
      }
#else
      if (!sema.CheckConstexprFunctionDecl(func)) {
        return true;
      }
#endif

      // We can't check this if we don't have a function body.
      if (!func->getBody()) {
        return true;
      }

#if LLVM_VERSION_MAJOR <= 9
      if (!sema.CheckConstexprFunctionBody(func, func->getBody())) {
        return true;
      }
#endif

      if (!CheckConstexprParameterTypes(sema, func)) {
        return true;
      }
    }

    SmallVector<PartialDiagnosticAt, 8> Diags;
    if (!Expr::isPotentialConstantExpr(func, Diags)) {
      return true;
    }

    // Mark function as constexpr, the next ast visitor will use this
    // information to find constexpr vardecls
#if LLVM_VERSION_MAJOR >= 12
    func->setConstexprKind(clang::ConstexprSpecKind::Constexpr);
#else
    func->setConstexprKind(CSK_unspecified);
#endif
    // Create diagnostic
    const auto FixIt = clang::FixItHint::CreateInsertion(loc, "constexpr ");
    const auto ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "function can be constexpr");

    DE.Report(loc, ID).AddFixItHint(FixIt);

    return true;
  }
};

class ConstexprVarDeclFunctionASTVisitor : public clang::RecursiveASTVisitor<ConstexprVarDeclFunctionASTVisitor>
{
  clang::SourceManager& sourceManager_;
  clang::CompilerInstance& CI_;
  clang::DiagnosticsEngine& DE;

  class ConstexprVarDeclVisitor : public clang::RecursiveASTVisitor<ConstexprVarDeclVisitor>
  {
    clang::CompilerInstance& CI_;
    clang::DiagnosticsEngine& DE;

   public:
    explicit ConstexprVarDeclVisitor(clang::CompilerInstance& ci) : CI_(ci), DE(ci.getASTContext().getDiagnostics()) {}

    bool VisitDeclStmt(clang::DeclStmt* stmt) {
      if (!stmt->isSingleDecl()) {
        return true;
      }

      auto* var = clang::dyn_cast<clang::VarDecl>(*stmt->decl_begin());
      if (!var) {
        return true;
      }

      // Skip variables that are already constexpr
      if (var->isConstexpr()) {
        return true;
      }

      // Only do locals for right now
      if (!var->hasLocalStorage()) {
        return true;
      }

      clang::SourceLocation const loc = stmt->getSourceRange().getBegin();
      // auto& sema = CI_.getSema();

      // var needs an initializer
      Expr* Init = var->getInit();
      if (!Init) {
        return true;
      }

      // If the var is const we can mark it constexpr
      QualType const ty = var->getType();
      if (!ty.isConstQualified()) {
        return true;
      }

      // Is init an integral constant expression
#if LLVM_VERSION_MAJOR >= 12
      if (!var->hasICEInitializer(stmt->getSingleDecl()->getASTContext())) {
#else
      if (!var->checkInitIsICE()) {
#endif
        return true;
      }

      // Does the init function use dependent values
      if (Init->isValueDependent()) {
        return true;
      }

      // Can we evaluate the value
      if (!var->evaluateValue()) {
        return true;
      }

      // Is init an ice
#if LLVM_VERSION_MAJOR >= 12
      if (!var->hasConstantInitialization()) {
#else
      if (!var->isInitICE()) {
#endif
        return true;
      }

      // Create Diagnostic/FixIt
      const auto FixIt = clang::FixItHint::CreateInsertion(loc, "constexpr ");
      const auto ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "variable can be constexpr");

      DE.Report(loc, ID).AddFixItHint(FixIt);

      return true;
    }
  };

 public:
  explicit ConstexprVarDeclFunctionASTVisitor(clang::SourceManager& sm, clang::CompilerInstance& ci)
    : sourceManager_(sm), CI_(ci), DE(ci.getASTContext().getDiagnostics()) {}

  bool VisitFunctionDecl(clang::FunctionDecl* func) {
    // Only functions in our TU
    SourceLocation const loc = func->getSourceRange().getBegin();
    if (!sourceManager_.isWrittenInMainFile(loc)) {
      return true;
    }

    // Don't go through functions that are already constexpr
    if (func->isConstexpr()) {
      return true;
    }

    ConstexprVarDeclVisitor vd(CI_);
    vd.TraverseFunctionDecl(func);

    return true;
  }
};

class ConstexprEverythingASTConsumer : public clang::ASTConsumer
{
  ConstexprFunctionASTVisitor functionVisitor;
  ConstexprVarDeclFunctionASTVisitor varDeclVisitor;

 public:
  // override the constructor in order to pass CI
  explicit ConstexprEverythingASTConsumer(clang::CompilerInstance& ci)
    : functionVisitor(ci.getSourceManager(), ci), varDeclVisitor(ci.getSourceManager(), ci) {}

  void HandleTranslationUnit(clang::ASTContext& astContext) override {
    functionVisitor.TraverseDecl(astContext.getTranslationUnitDecl());
    varDeclVisitor.TraverseDecl(astContext.getTranslationUnitDecl());
  }
};

class FunctionDeclFrontendAction : public clang::ASTFrontendAction
{

  class ConstexprFixItOptions : public clang::FixItOptions
  {
    std::string RewriteFilename(const std::string& Filename, int& /*fd*/) override {
      return Filename;
    }
  };

  std::unique_ptr<clang::FixItRewriter> rewriter = nullptr;
  bool inPlaceRewrite;

 public:
  FunctionDeclFrontendAction() : inPlaceRewrite(ConstExprFixItOption) {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, clang::StringRef /*file*/) override {

    if (inPlaceRewrite) {
      ConstexprFixItOptions fixItOptions;
      fixItOptions.InPlace = inPlaceRewrite;

      rewriter = std::make_unique<clang::FixItRewriter>(CI.getDiagnostics(), CI.getASTContext().getSourceManager(), CI.getASTContext().getLangOpts(),
                                                        &fixItOptions);

      CI.getDiagnostics().setClient(rewriter.get(), false);
    }

    return std::make_unique<ConstexprEverythingASTConsumer>(CI);  // pass CI pointer to ASTConsumer
  }

  void EndSourceFileAction() override {
    if (inPlaceRewrite) {
      rewriter->WriteFixedFiles();
    }
  }
};

int main(int argc, const char** argv) {

#if LLVM_VERSION_MAJOR >= 13
  auto ExpectedParser =
    CommonOptionsParser::create(argc, argv, ConstexprCategory, llvm::cl::ZeroOrMore, "Clang-based refactoring tool for constexpr everything");
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = ExpectedParser.get();
#else
  CommonOptionsParser OptionsParser(argc, argv, ConstexprCategory);
#endif

  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  Tool.run(newFrontendActionFactory<FunctionDeclFrontendAction>().get());
}
