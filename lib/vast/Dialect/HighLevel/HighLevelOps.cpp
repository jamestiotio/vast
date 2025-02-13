// Copyright (c) 2021-present, Trail of Bits, Inc.

#include "vast/Dialect/HighLevel/HighLevelAttributes.hpp"
#include "vast/Dialect/HighLevel/HighLevelDialect.hpp"
#include "vast/Dialect/HighLevel/HighLevelTypes.hpp"
#include "vast/Dialect/HighLevel/HighLevelOps.hpp"

#include "vast/Util/Common.hpp"

#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/OpImplementation.h>
#include <mlir/IR/FunctionImplementation.h>

#include <llvm/Support/ErrorHandling.h>

#include <optional>
#include <variant>

namespace vast::hl
{
    namespace detail
    {
        Region* build_region(Builder &bld, State &st, BuilderCallback callback)
        {
            auto reg = st.addRegion();
            if (callback.has_value()) {
                bld.createBlock(reg);
                callback.value()(bld, st.location);
            }
            return reg;
        }
    } // namespace detail

    using FoldResult = mlir::OpFoldResult;

    //===----------------------------------------------------------------------===//
    // FuncOp
    //===----------------------------------------------------------------------===//

    // This function is adapted from CIR:
    //
    // Verifies linkage types, similar to LLVM:
    // - functions don't have 'common' linkage
    // - external functions have 'external' or 'extern_weak' linkage
    LogicalResult FuncOp::verify() {
        auto linkage = getLinkage();
        constexpr auto common = GlobalLinkageKind::CommonLinkage;
        if (linkage == common) {
            return emitOpError() << "functions cannot have '"
                << stringifyGlobalLinkageKind(common)
                << "' linkage";
        }

        if (isExternal()) {
            constexpr auto external = GlobalLinkageKind::ExternalLinkage;
            constexpr auto weak_external = GlobalLinkageKind::ExternalWeakLinkage;
            if (linkage != external && linkage != weak_external) {
                return emitOpError() << "external functions must have '"
                    << stringifyGlobalLinkageKind(external)
                    << "' or '"
                    << stringifyGlobalLinkageKind(weak_external)
                    << "' linkage";
            }
            return mlir::success();
        }
        return mlir::success();
    }

    void add_arg_attrs(Builder &bld, State &st, llvm::ArrayRef< mlir::DictionaryAttr > arg_attrs) {
        auto non_empty_attrs = [] (mlir::DictionaryAttr attrs) {
            return attrs && !attrs.empty();
        };

        // Convert the specified array of dictionary attrs (which may have null
        // entries) to an ArrayAttr of dictionaries.
        auto get_array_attr = [&] (llvm::ArrayRef< mlir::DictionaryAttr > dict_attrs) {
            llvm::SmallVector< Attribute > attrs;
            for (auto &dict : dict_attrs) {
                attrs.push_back(dict ? dict : bld.getDictionaryAttr({}));
            }
            return bld.getArrayAttr(attrs);
        };

        // Add the attributes to the function arguments.
        if (llvm::any_of(arg_attrs, non_empty_attrs)) {
            st.addAttribute(mlir::function_interface_impl::getArgDictAttrName(), get_array_attr(arg_attrs));
        }
    }

    ParseResult parseFunctionSignaruteAndBody(
        Parser &parser, Attribute &funcion_type, mlir::NamedAttrList &attr_dict, Region &body
    ) {
        llvm::SmallVector< Parser::Argument, 8 > arguments;
        llvm::SmallVector< mlir::DictionaryAttr, 1 > result_attrs;
        llvm::SmallVector< Type, 8 > arg_types;
        llvm::SmallVector< Type, 4 > result_types;

        auto &builder = parser.getBuilder();

        bool is_variadic = false;
        if (mlir::failed(mlir::function_interface_impl::parseFunctionSignature(
            parser, /*allowVariadic=*/false, arguments, is_variadic, result_types, result_attrs
        ))) {
            return mlir::failure();
        }


        for (auto &arg : arguments) {
            arg_types.push_back(arg.type);
        }

        // create parsed function type
        funcion_type = mlir::TypeAttr::get(
            builder.getFunctionType(arg_types, result_types)
        );

        // If additional attributes are present, parse them.
        if (parser.parseOptionalAttrDictWithKeyword(attr_dict)) {
            return mlir::failure();
        }

        // TODO: Add the attributes to the function arguments.
        // VAST_ASSERT(result_attrs.size() == result_types.size());
        // return mlir::function_interface_impl::addArgAndResultAttrs(
        //     builder, state, arguments, result_attrs
        // );

        auto loc = parser.getCurrentLocation();
        auto parse_result = parser.parseOptionalRegion(
            body, arguments, /* enableNameShadowing */false
        );

        if (parse_result.hasValue()) {
            if (failed(*parse_result))
                return mlir::failure();
            // Function body was parsed, make sure its not empty.
            if (body.empty())
                return parser.emitError(loc, "expected non-empty function body");
        }

        return mlir::success();
    }

    void printFunctionSignaruteAndBody(
        Printer &printer, FuncOp op, Attribute /* funcion_type */, mlir::DictionaryAttr, Region &body
    ) {
        auto fty = op.getFunctionType();
        mlir::function_interface_impl::printFunctionSignature(
            printer, op, fty.getInputs(), /* variadic */false, fty.getResults()
        );

        mlir::function_interface_impl::printFunctionAttributes(
            printer, op, fty.getNumInputs(), fty.getNumResults(), {"linkage"}
        );

        if (!body.empty()) {
            printer.getStream() << " ";
            printer.printRegion( body,
                /* printEntryBlockArgs */false,
                /* printBlockTerminators */true
            );
        }
    }

    FoldResult ConstantOp::fold(mlir::ArrayRef<Attribute> operands) {
        VAST_CHECK(operands.empty(), "constant has no operands");
        return getValue();
    }


    void build_expr_trait(Builder &bld, State &st, Type rty, BuilderCallback expr) {
        VAST_ASSERT(expr && "the builder callback for 'expr' block must be present");
        Builder::InsertionGuard guard(bld);
        detail::build_region(bld, st, expr);
        st.addTypes(rty);
    }

    void SizeOfExprOp::build(Builder &bld, State &st, Type rty, BuilderCallback expr) {
        build_expr_trait(bld, st, rty, expr);
    }

    void AlignOfExprOp::build(Builder &bld, State &st, Type rty, BuilderCallback expr) {
        build_expr_trait(bld, st, rty, expr);
    }

    void VarDeclOp::build(Builder &bld, State &st, Type type, llvm::StringRef name, BuilderCallback init, BuilderCallback alloc) {
        st.addAttribute("name", bld.getStringAttr(name));
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, init);
        detail::build_region(bld, st, alloc);

        st.addTypes(type);
    }

    void EnumDeclOp::build(Builder &bld, State &st, llvm::StringRef name, Type type, BuilderCallback constants) {
        st.addAttribute("name", bld.getStringAttr(name));
        st.addAttribute("type", mlir::TypeAttr::get(type));
        Builder::InsertionGuard guard(bld);
        detail::build_region(bld, st, constants);
    }

    namespace detail {
        void build_record_like_decl(Builder &bld, State &st, llvm::StringRef name, BuilderCallback fields) {
            st.addAttribute("name", bld.getStringAttr(name));

            Builder::InsertionGuard guard(bld);
            detail::build_region(bld, st, fields);
        }
    } // anamespace detail

    void StructDeclOp::build(Builder &bld, State &st, llvm::StringRef name, BuilderCallback fields) {
        detail::build_record_like_decl(bld, st, name, fields);
    }

    void UnionDeclOp::build(Builder &bld, State &st, llvm::StringRef name, BuilderCallback fields) {
        detail::build_record_like_decl(bld, st, name, fields);
    }

    mlir::CallInterfaceCallable CallOp::getCallableForCallee()
    {
        return (*this)->getAttrOfType< mlir::SymbolRefAttr >("callee");
    }

    mlir::CallInterfaceCallable IndirectCallOp::getCallableForCallee()
    {
        return (*this)->getOperand(0);
    }

    void IfOp::build(Builder &bld, State &st, BuilderCallback condBuilder, BuilderCallback thenBuilder, BuilderCallback elseBuilder)
    {
        VAST_ASSERT(condBuilder && "the builder callback for 'condition' block must be present");
        VAST_ASSERT(thenBuilder && "the builder callback for 'then' block must be present");

        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, condBuilder);
        detail::build_region(bld, st, thenBuilder);
        detail::build_region(bld, st, elseBuilder);
    }

    void WhileOp::build(Builder &bld, State &st, BuilderCallback cond, BuilderCallback body)
    {
        VAST_ASSERT(cond && "the builder callback for 'condition' block must be present");
        VAST_ASSERT(body && "the builder callback for 'body' must be present");

        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, cond);
        detail::build_region(bld, st, body);
    }

    void ForOp::build(Builder &bld, State &st, BuilderCallback cond, BuilderCallback incr, BuilderCallback body)
    {
        VAST_ASSERT(body && "the builder callback for 'body' must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, cond);
        detail::build_region(bld, st, incr);
        detail::build_region(bld, st, body);
    }

    void DoOp::build(Builder &bld, State &st, BuilderCallback body, BuilderCallback cond)
    {
        VAST_ASSERT(body && "the builder callback for 'body' must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, body);
        detail::build_region(bld, st, cond);
    }

    void SwitchOp::build(Builder &bld, State &st, BuilderCallback cond, BuilderCallback body)
    {
        VAST_ASSERT(cond && "the builder callback for 'condition' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, cond);
        detail::build_region(bld, st, body);
    }

    void CaseOp::build(Builder &bld, State &st, BuilderCallback lhs, BuilderCallback body)
    {
        VAST_ASSERT(lhs && "the builder callback for 'case condition' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, lhs);
        detail::build_region(bld, st, body);
    }

    void DefaultOp::build(Builder &bld, State &st, BuilderCallback body)
    {
        VAST_ASSERT(body && "the builder callback for 'body' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, body);
    }

    void LabelStmt::build(Builder &bld, State &st, Value label, BuilderCallback substmt)
    {
        st.addOperands(label);

        VAST_ASSERT(substmt && "the builder callback for 'substmt' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, substmt);
    }

    void ExprOp::build(Builder &bld, State &st, Type rty, std::unique_ptr< Region > &&region) {
        Builder::InsertionGuard guard(bld);
        st.addRegion(std::move(region));
        st.addTypes(rty);
    }

    LogicalResult IndirectCallOp::inferReturnTypes(
        MContext *context, mlir::Optional<Location> location, mlir::ValueRange operands,
        mlir::DictionaryAttr attributes, mlir::RegionRange regions,
        mlir::SmallVectorImpl<Type> &inferredReturnTypes
    ) {
        auto callee = operands[0];
        auto type = getFunctionType(callee);
        inferredReturnTypes.assign({type.getResult(0)});
        return mlir::success();
    }
}

//===----------------------------------------------------------------------===//
// TableGen generated logic.
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "vast/Dialect/HighLevel/HighLevel.cpp.inc"
