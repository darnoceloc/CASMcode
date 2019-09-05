#ifndef CASM_ClusterSymCompare
#define CASM_ClusterSymCompare

#include "casm/symmetry/SymCompare.hh"

namespace CASM {

  template<typename Derived>
  class ClusterSymCompare;
  class PrimGrid;
  class PrimClex;
  class Supercell;

  /// \brief Traits class for ClusterSymCompare
  ///
  /// \ingroup IntegralCluster
  ///
  template<typename Derived>
  struct traits<ClusterSymCompare<Derived>> : public traits<Derived> {};

  /// \brief CRTP Base class for Cluster comparisons
  ///
  /// Implements:
  /// - 'invariants_compare_impl' using 'compare'
  /// - 'compare_impl'
  /// - 'canonical_transform_impl'
  ///
  /// Does not implement:
  /// - 'spatial_prepare_impl'
  /// - 'representation_prepare_impl'
  ///
  /// traits<Element> requires:
  /// - static UnitCellCoord position(const Element& el) const;
  /// - typedef <ElementInvariants> InvariantsType;
  ///
  /// The ClusterSymCompare hierarchy:
  /// - SymCompare
  ///   - ClusterSymCompare
  ///     - IntegralClusterSymCompare (implements 'compare_impl')
  ///       - LocalSymCompare<IntegralCluster> (implements 'prepare_impl')
  ///       - PrimPeriodicSymCompare<IntegralCluster> (implements 'prepare_impl')
  ///       - ScelPeriodicSymCompare<IntegralCluster> (implements 'prepare_impl')
  ///
  /// \ingroup Clusterography
  ///
  template<typename Base>
  class ClusterSymCompare : public Base {

  public:

    /// Element refers to Cluster, not element of Cluster
    /*
    typedef typename traits<Derived>::MostDerived MostDerived;
    typedef typename traits<Derived>::Element Element;
    typedef Element ClusterType;
    typedef typename traits<Element>::InvariantsType InvariantsType;
    */

    typedef typename Base::MostDerived MostDerived;
    typedef typename traits<MostDerived>::Element Element;
    typedef Element ClusterType;
    typedef typename traits<Element>::InvariantsType InvariantsType;
    using Base::derived;

    /// \brief Return tolerance
    double tol() const;

  protected:

    /// \brief Constructor
    ///
    /// \param tol Tolerance for invariants_compare of site-to-site distances
    ///
    ClusterSymCompare(double tol);


    /// \brief Orders 'prepared' elements in the same orbit
    bool invariants_compare_impl(const Element &A, const Element &B) const;

    /// \brief Compares 'prepared' elements
    bool compare_impl(const Element &A, const Element &B) const;

    /// \brief Returns transformation that takes 'obj' to its prepared (canonical) form
    // For now, this is the the sorting permutation
    std::unique_ptr<SymOpRepresentation> canonical_transform_impl(Element const &obj)const;

  protected:

    /// \brief type-specific way to get position of element
    ///
    /// - Returns traits<Element>::position(el)
    static UnitCellCoord position(const Element &el);

  private:

    double m_tol;

  };

  template <
    typename ClusterType,
    typename = typename std::enable_if <
      std::is_same <
        UnitCellCoord,
        decltype(traits<ClusterType>::position(std::declval<ClusterType>()))
        >::value >::type >
  using enable_if_integral_position = ClusterType;


  /* -- LocalSymCompare<IntegralCluster> Declaration ------------------------------------- */

  /// \brief Comparisons of GenericCluster-derived types using aperiodic symmetry
  ///
  /// The ClusterSymCompare hierarchy:
  /// - SymCompare<Derived>
  ///   - ClusterSymCompare<Derived> (implements 'compare_impl', 'invariants_compare_impl')
  ///     - AperiodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - PrimPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - ScelPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///
  /// \ingroup IntegralCluster
  ///
  template<typename Element>
  class AperiodicSymCompare<Element/*, enable_if_integral_position<Element>*/> :
    public ClusterSymCompare<SymCompare<CRTPBase<AperiodicSymCompare<Element>>>> {

  public:

    typedef ClusterSymCompare<SymCompare<CRTPBase<AperiodicSymCompare<Element>>>> Base;
    using Base::position;

    /// \brief Constructor
    ///
    /// \param tol Tolerance for invariants_compare of site-to-site distances
    ///
    AperiodicSymCompare(double tol);

  protected:
    friend SymCompare<CRTPBase<AperiodicSymCompare<Element>>>;

    /// \brief Prepare an element for comparison via an isometric affine transformation
    ///
    /// - For aperiodic cases, no isometric transformations are allowed, so apply and return identity
    Element spatial_prepare_impl(Element obj) const;


    /// \brief Prepare an element for comparison via transformation of its internal representation
    ///
    /// - Returns sorted
    Element representation_prepare_impl(Element obj) const;

  };


  /// \brief Comparisons of GenericCluster-derived types using prim periodic symmetry
  ///
  /// The ClusterSymCompare hierarchy:
  /// - SymCompare<Derived>
  ///   - ClusterSymCompare<Derived> (implements 'compare_impl', 'invariants_compare_impl')
  ///     - LocalSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - PrimPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - ScelPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///
  /// \ingroup IntegralCluster
  ///
  template<typename Element>
  class PrimPeriodicSymCompare<Element/*, enable_if_integral_position<Element>*/> :
    public ClusterSymCompare<SymCompare<CRTPBase<PrimPeriodicSymCompare<Element>>>> {

  public:

    typedef ClusterSymCompare<SymCompare<CRTPBase<PrimPeriodicSymCompare<Element>>>> Base;
    using Base::position;

    /// \brief Constructor
    ///
    /// \param tol Tolerance for invariants_compare of site-to-site distances
    ///
    PrimPeriodicSymCompare(double tol);

    PrimPeriodicSymCompare(const PrimClex &primclex);

  protected:
    friend SymCompare<CRTPBase<PrimPeriodicSymCompare<Element>>>;

    /// \brief Prepare an element for comparison via an isometric affine transformation
    ///
    /// - Applies lattice translation such that first site of cluster is in UnitCell (0,0,0)
    Element spatial_prepare_impl(Element obj) const;

    /// \brief Prepare an element for comparison via transformation of its internal representation
    ///
    /// - Returns sorted
    Element representation_prepare_impl(Element obj) const;

  };

  /// \brief Comparisons of GenericCluster-derived types using supercell periodic symmetry
  ///
  /// The ClusterSymCompare hierarchy:
  /// - SymCompare<Derived>
  ///   - ClusterSymCompare<Derived> (implements 'compare_impl', 'invariants_compare_impl')
  ///     - LocalSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - PrimPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - ScelPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///
  /// \ingroup IntegralCluster
  ///
  template<typename Element>
  class ScelPeriodicSymCompare<Element/*, enable_if_integral_position<Element>*/> :
    public ClusterSymCompare<SymCompare<CRTPBase<ScelPeriodicSymCompare<Element>>>> {

  public:

    typedef ClusterSymCompare<SymCompare<CRTPBase<ScelPeriodicSymCompare<Element>>>> Base;
    using Base::position;

    /// \brief Constructor
    ///
    /// \param tol Tolerance for invariants_compare of site-to-site distances
    ///
    ScelPeriodicSymCompare(const PrimGrid &prim_grid, double tol);

    /// \brief Constructor
    ScelPeriodicSymCompare(const Supercell &scel);

  protected:
    friend SymCompare<CRTPBase<ScelPeriodicSymCompare<Element>>>;

    /// \brief Prepare an element for comparison via an isometric affine transformation
    ///
    /// - Applies superlattice translation such that first site of cluster is within supercell
    Element spatial_prepare_impl(Element obj) const;

    /// \brief Prepare an element for comparison via transformation of its internal representation
    ///
    /// - Returns sorted
    Element representation_prepare_impl(Element obj) const;

    const PrimGrid *m_prim_grid;
  };

  /// \brief Comparisons of GenericCluster-derived types using supercell periodic symmetry
  ///
  /// The ClusterSymCompare hierarchy:
  /// - SymCompare<Derived>
  ///   - ClusterSymCompare<Derived> (implements 'compare_impl', 'invariants_compare_impl')
  ///     - LocalSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - PrimPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - ScelPeriodicSymCompare<ClusterType> (implements 'prepare_impl')
  ///     - WithinScelSymCompare<ClusterType> (implements 'prepare_impl')
  ///
  /// \ingroup IntegralCluster
  ///
  template<typename Element>
  class WithinScelSymCompare<Element/*, enable_if_integral_position<Element>*/> :
    public ClusterSymCompare<SymCompare<CRTPBase<WithinScelSymCompare<Element>>>> {

  public:

    typedef ClusterSymCompare<SymCompare<CRTPBase<WithinScelSymCompare<Element>>>> Base;
    using Base::position;

    /// \brief Constructor
    ///
    /// \param tol Tolerance for invariants_compare of site-to-site distances
    ///
    WithinScelSymCompare(const PrimGrid &prim_grid, double tol);

    /// \brief Constructor
    WithinScelSymCompare(const Supercell &scel);

  protected:
    friend SymCompare<CRTPBase<WithinScelSymCompare<Element>>>;

    /// \brief Returns transformation that takes 'obj' to its prepared (canonical) form
    ///
    std::unique_ptr<SymOpRepresentation> canonical_transform_impl(Element const &obj)const;

    /// \brief Prepare an element for comparison via an isometric affine transformation
    ///
    /// - Applies superlattice translation such that first site of cluster is within supercell
    Element spatial_prepare_impl(Element obj) const;


    /// \brief Prepare an element for comparison via transformation of its internal representation
    ///
    /// - Returns sorted
    Element representation_prepare_impl(Element obj) const;

    const PrimGrid *m_prim_grid;
  };

}

#endif