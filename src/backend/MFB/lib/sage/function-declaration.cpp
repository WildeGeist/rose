
#include "MFB/Sage/function-declaration.hpp"
#include "MFB/Sage/namespace-declaration.hpp"

#include "sage3basic.h"

#ifndef PATCHING_SAGE_BUILDER_ISSUES
#  define PATCHING_SAGE_BUILDER_ISSUES 1
#endif

namespace MultiFileBuilder {

bool ignore(const std::string & name);
bool ignore(SgScopeStatement * scope);

template <>
bool Driver<Sage>::resolveValidParent<SgFunctionSymbol>(SgFunctionSymbol * symbol) {
  SgNamespaceSymbol * parent = NULL;
  
  if (p_valid_symbols.find(symbol) != p_valid_symbols.end()) return true;

  SgNamespaceDefinitionStatement * namespace_scope = isSgNamespaceDefinitionStatement(symbol->get_scope());
  if (namespace_scope != NULL) {
    SgNamespaceDeclarationStatement * parent_decl = namespace_scope->get_namespaceDeclaration();
    assert(parent_decl != NULL);
    parent = SageInterface::lookupNamespaceSymbolInParentScopes(parent_decl->get_name(), parent_decl->get_scope());
    assert(parent != NULL);

    if (!resolveValidParent<SgNamespaceSymbol>(parent)) return false;
    assert(p_valid_symbols.find(parent) != p_valid_symbols.end());
  }

  p_valid_symbols.insert(symbol);
  p_parent_map.insert(std::pair<SgSymbol *, SgSymbol *>(symbol, parent));
  p_function_symbols.insert(symbol);

  return true;
}

template <>
void  Driver<Sage>::loadSymbolsFromPair<SgFunctionDeclaration>(unsigned long file_id, SgSourceFile * header_file, SgSourceFile * source_file) {
  std::vector<SgFunctionDeclaration *> header_function_decl = SageInterface::querySubTree<SgFunctionDeclaration>(header_file);
  std::vector<SgFunctionDeclaration *> source_function_decl = SageInterface::querySubTree<SgFunctionDeclaration>(source_file);

  std::set<SgFunctionSymbol *> function_symbols;
  std::vector<SgFunctionDeclaration *>::const_iterator it_function_decl;
  for (it_function_decl = header_function_decl.begin(); it_function_decl != header_function_decl.end(); it_function_decl++) {
    SgFunctionDeclaration * function_decl = *it_function_decl;

    if (ignore(function_decl->get_scope())) continue;
    if (ignore(function_decl->get_name().getString())) continue;

    SgFunctionSymbol * function_sym = SageInterface::lookupFunctionSymbolInParentScopes(function_decl->get_name(), function_decl->get_scope());
    assert(function_sym != NULL);

    function_symbols.insert(function_sym);
  }
  for (it_function_decl = source_function_decl.begin(); it_function_decl != source_function_decl.end(); it_function_decl++) {
    SgFunctionDeclaration * function_decl = *it_function_decl;

    if (ignore(function_decl->get_scope())) continue;
    if (ignore(function_decl->get_name().getString())) continue;

    SgFunctionSymbol * function_sym = SageInterface::lookupFunctionSymbolInParentScopes(function_decl->get_name(), function_decl->get_scope());
    assert(function_sym != NULL);

    function_symbols.insert(function_sym);
  }

  std::set<SgFunctionSymbol *>::iterator it;
  for (it = function_symbols.begin(); it != function_symbols.end(); it++)
    if (resolveValidParent<SgFunctionSymbol>(*it)) {
      p_symbol_to_file_id_map.insert(std::pair<SgSymbol *, unsigned long>(*it, file_id));
//    std::cout << " Function Symbol : " << (*it) << ", name = " << (*it)->get_name().getString() << ", scope = " << (*it)->get_scope() << "(" << (*it)->get_scope()->class_name() << ")" << std::endl;
    }
}

Sage<SgFunctionDeclaration>::object_desc_t::object_desc_t(
  std::string name_,
  SgType * return_type_,
  SgFunctionParameterList * params_,
  SgNamespaceSymbol * parent_namespace_,
  unsigned long file_id_,
  bool is_static_, 
  bool create_definition_
) :
  name(name_),
  return_type(return_type_),
  params(params_),
  parent(parent_namespace_),
  file_id(file_id_),
  is_static(is_static_),
  create_definition(create_definition_)
{}

template <>
Sage<SgFunctionDeclaration>::build_result_t Driver<Sage>::build<SgFunctionDeclaration>(const Sage<SgFunctionDeclaration>::object_desc_t & desc) {
  Sage<SgFunctionDeclaration>::build_result_t result;

  Sage<SgFunctionDeclaration>::build_scopes_t scopes = getBuildScopes<SgFunctionDeclaration>(desc);

  SgScopeStatement * decl_scope = scopes.decl_scope;
  SgScopeStatement * defn_scope = scopes.defn_scope;

  SgFunctionDeclaration * func_decl = NULL;
  {
    func_decl = SageBuilder::buildNondefiningFunctionDeclaration(desc.name, desc.return_type, desc.params, decl_scope, NULL, false, NULL);
    SageInterface::appendStatement(func_decl, decl_scope);

    assert(func_decl->get_definition() == NULL);
  }

  SgFunctionDeclaration * func_defn = NULL;
  {
    func_defn = SageBuilder::buildDefiningFunctionDeclaration(desc.name, desc.return_type, desc.params, defn_scope, NULL, false, func_decl, NULL);
    SageInterface::appendStatement(func_defn, defn_scope);
  
    result.definition = func_defn->get_definition();
    assert(result.definition != NULL);
  }

  {
    result.symbol = decl_scope->lookup_function_symbol(desc.name);
    assert(result.symbol != NULL);

    p_symbol_to_file_id_map.insert(std::pair<SgSymbol *, unsigned long>(result.symbol, desc.file_id));
    p_valid_symbols.insert(result.symbol);
    p_parent_map.insert(std::pair<SgSymbol *, SgSymbol *>(result.symbol, desc.parent));
    p_function_symbols.insert(result.symbol);
  }

#if PATCHING_SAGE_BUILDER_ISSUES
  {
    func_decl->set_definingDeclaration(func_defn);
    func_decl->set_firstNondefiningDeclaration(func_decl);

    func_defn->set_definingDeclaration(func_defn);
    func_defn->set_firstNondefiningDeclaration(func_decl);
  }
#endif

  return result;
}

template <>
Sage<SgFunctionDeclaration>::build_scopes_t Driver<Sage>::getBuildScopes<SgFunctionDeclaration>(const Sage<SgFunctionDeclaration>::object_desc_t & desc) {
  Sage<SgFunctionDeclaration>::build_scopes_t result;

  assert(desc.file_id != 0);

  std::map<unsigned long, std::pair<SgSourceFile *, SgSourceFile *> >::iterator it_file_pair = file_pair_map.find(desc.file_id);
  std::map<unsigned long, SgSourceFile *>::iterator it_standalone_source_file = standalone_source_file_map.find(desc.file_id);

  assert((it_file_pair != file_pair_map.end()) xor (it_standalone_source_file != standalone_source_file_map.end()));

  SgSourceFile * decl_file = NULL;
  SgSourceFile * defn_file = NULL;
  if (it_file_pair != file_pair_map.end()) {
    decl_file = it_file_pair->second.first;
    defn_file = it_file_pair->second.second;
  }
  else if (it_standalone_source_file != standalone_source_file_map.end()) {
    decl_file = it_standalone_source_file->second;
    defn_file = it_standalone_source_file->second;
  }
  else assert(false);

  assert(decl_file != NULL && defn_file != NULL);

  if (desc.parent == NULL) {
    result.decl_scope = decl_file->get_globalScope();
    result.defn_scope = defn_file->get_globalScope();
  }
  else {
    result.decl_scope = Sage<SgNamespaceDeclarationStatement>::getDefinition(desc.parent, decl_file);
    result.defn_scope = Sage<SgNamespaceDeclarationStatement>::getDefinition(desc.parent, defn_file);
  }

  assert(result.decl_scope != NULL && result.defn_scope != NULL);

  return result;
}

template <>
void Driver<Sage>::createForwardDeclaration<SgFunctionDeclaration>(Sage<SgFunctionDeclaration>::symbol_t symbol, SgSourceFile * target_file) {
  assert(false);
}

}
