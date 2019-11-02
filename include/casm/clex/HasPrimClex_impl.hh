#ifndef CASM_HasPrimClex_impl
#define CASM_HasPrimClex_impl

#include "casm/clex/PrimClex.hh"
#include "casm/clex/HasPrimClex.hh"

namespace CASM {

  template<typename Base>
  const Structure &HasPrimClex<Base>::prim() const {
    return derived().primclex().prim();
  }

  template<typename Base>
  double HasPrimClex<Base>::crystallography_tol() const {
    return derived().primclex().crystallography_tol();
  }

}

#endif
