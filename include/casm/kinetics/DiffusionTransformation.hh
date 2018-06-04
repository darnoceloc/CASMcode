#ifndef CASM_DiffusionTransformation
#define CASM_DiffusionTransformation

#include "casm/misc/cloneable_ptr.hh"
#include "casm/misc/Comparisons.hh"
#include "casm/symmetry/SymCompare.hh"
#include "casm/crystallography/UnitCellCoord.hh"
#include "casm/clusterography/ClusterInvariants.hh"
#include "casm/clusterography/IntegralCluster.hh"
#include "casm/clex/HasCanonicalForm.hh"
#include "casm/kinetics/DiffusionTransformationTraits.hh"
#include "casm/kinetics/DoFTransformation.hh"
#include "casm/kinetics/OccupationTransformation.hh"

namespace CASM {

  class AtomSpecies;
  class Structure;
  class Configuration;
  class SymOp;
  class jsonParser;
  class PermuteIterator;
  template<typename T> struct jsonConstructor;

  namespace Kinetics {

    /// \brief Specifies a particular species
    struct SpeciesLocation : public Comparisons<CRTPBase<SpeciesLocation>> {

      SpeciesLocation(const UnitCellCoord &_uccoord, Index _occ, Index _pos);

      UnitCellCoord uccoord;

      /// Occupant index
      Index occ;

      /// Position of species in Molecule
      Index pos;

      bool operator<(const SpeciesLocation &B) const;

      const Molecule &mol() const;

      const AtomSpecies &species() const;

    private:

      std::tuple<UnitCellCoord, Index, Index> _tuple() const;
    };

    /// \brief Print DiffTransInvariants
    std::ostream &operator<<(std::ostream &sout, const SpeciesLocation &obj);

  }

  jsonParser &to_json(const Kinetics::SpeciesLocation &obj, jsonParser &json);

  template<>
  struct jsonConstructor<Kinetics::SpeciesLocation> {

    static Kinetics::SpeciesLocation from_json(const jsonParser &json, const Structure &prim);
  };

  void from_json(Kinetics::SpeciesLocation &obj, const jsonParser &json);


  namespace Kinetics {

    /// \brief Describes how one species moves
    class SpecieTrajectory : public Comparisons<CRTPBase<SpecieTrajectory>> {

    public:

      SpecieTrajectory(const SpeciesLocation &_from, const SpeciesLocation &_to);

      SpecieTrajectory &operator+=(UnitCell frac);

      SpecieTrajectory &operator-=(UnitCell frac);

      bool species_types_map() const;

      bool is_no_change() const;

      SpeciesLocation from;
      SpeciesLocation to;

      bool operator<(const SpecieTrajectory &B) const;

      SpecieTrajectory &apply_sym(const SymOp &op);

      void reverse();

    private:

      std::tuple<SpeciesLocation, SpeciesLocation> _tuple() const;

    };
  }

  jsonParser &to_json(const Kinetics::SpecieTrajectory &traj, jsonParser &json);

  template<>
  struct jsonConstructor<Kinetics::SpecieTrajectory> {

    static Kinetics::SpecieTrajectory from_json(const jsonParser &json, const Structure &prim);
  };

  void from_json(Kinetics::SpecieTrajectory &traj, const jsonParser &json);


  namespace Kinetics {

    class DiffusionTransformation;

    /// \brief Invariants of a DiffusionTransformation, used to sort orbits
    class DiffTransInvariants {

    public:

      DiffTransInvariants(const DiffusionTransformation &trans);

      ClusterInvariants<IntegralCluster> cluster_invariants;
      std::map<AtomSpecies, Index> species_count;

    };
  }


  /// \brief Check if DiffTransInvariants are equal
  bool almost_equal(const Kinetics::DiffTransInvariants &A,
                    const Kinetics::DiffTransInvariants &B,
                    double tol);

  /// \brief Compare DiffTransInvariants
  bool compare(const Kinetics::DiffTransInvariants &A,
               const Kinetics::DiffTransInvariants &B,
               double tol);

  /// \brief Print DiffTransInvariants
  std::ostream &operator<<(std::ostream &sout,
                           const Kinetics::DiffTransInvariants &obj);


  namespace Kinetics {

    typedef DoFTransformation <
    CanonicalForm <
    Comparisons <
    Translatable <
    SymComparable <
    CRTPBase<DiffusionTransformation >>> >>> DiffTransBase;

    /// \brief Describes how species move
    class DiffusionTransformation : public DiffTransBase {

    public:

      DiffusionTransformation(const Structure &_prim);


      const Structure &prim() const;

      DiffusionTransformation &operator+=(UnitCell frac);

      bool is_valid_occ_transform() const;

      /// \brief Check species_types_map() && !breaks_indivisible_mol() && !is_subcluster_transformation()
      bool is_valid_species_traj() const;

      bool species_types_map() const;

      bool breaks_indivisible_mol() const;

      bool is_subcluster_transformation() const;

      /// \brief Check if species_traj() and occ_transform() are consistent
      bool is_self_consistent() const;

      bool is_valid() const;

      std::vector<OccupationTransformation> &occ_transform();
      const std::vector<OccupationTransformation> &occ_transform() const;

      std::vector<SpecieTrajectory> &species_traj();
      const std::vector<SpecieTrajectory> &species_traj() const;

      const IntegralCluster &cluster() const;
      const std::map<AtomSpecies, Index> &species_count() const;

      /// \brief Compare DiffusionTransformation
      ///
      /// - Comparison is made using the sorted forms
      bool operator<(const DiffusionTransformation &B) const;

      Permutation sort_permutation() const;

      DiffusionTransformation &sort();

      DiffusionTransformation sorted() const;

      bool is_sorted() const;

      /// \brief Return the cluster size
      Index size() const;

      /// \brief Return the min pair distance, or 0.0 if size() <= 1
      double min_length() const;

      /// \brief Return the max pair distance, or 0.0 if size() <= 1
      double max_length() const;

      DiffusionTransformation &apply_sym(const SymOp &op);

      DiffusionTransformation &apply_sym(const PermuteIterator &it);

      Configuration &apply_to(Configuration &config) const;

      void reverse();


    private:

      friend DiffTransBase;

      Configuration &apply_reverse_to_impl(Configuration &config) const;

      void _forward_sort();

      bool _lt(const DiffusionTransformation &B) const;

      /// \brief Reset mutable members, cluster and invariants, when necessary
      void _reset();

      std::map<AtomSpecies, Index> _from_species_count() const;
      std::map<AtomSpecies, Index> _to_species_count() const;

      const Structure *m_prim_ptr;

      std::vector<OccupationTransformation> m_occ_transform;
      std::vector<SpecieTrajectory> m_species_traj;

      // stores IntegralCluster, based on occ_transform uccoord
      mutable notstd::cloneable_ptr<IntegralCluster> m_cluster;

      // stores Specie -> count, using 'from' species
      // - is equal to 'to' species count if is_valid_occ_transform() == true
      mutable notstd::cloneable_ptr<std::map<AtomSpecies, Index> > m_species_count;

    };

    /// \brief Print DiffusionTransformation to stream, using default Printer<Kinetics::DiffusionTransformation>
    std::ostream &operator<<(std::ostream &sout, const DiffusionTransformation &trans);

    /// \brief Return a standardized name for this diffusion transformation orbit
    //std::string orbit_name(const PrimPeriodicDiffTransOrbit &orbit);

    // \brief Returns the distance from uccoord to the closest point on a linearly
    /// interpolated diffusion path. (Could be an end point)
    double dist_to_path(const DiffusionTransformation &diff_trans, const UnitCellCoord &uccoord);

    // \brief Returns the vector from uccoord to the closest point on a linearly
    /// interpolated diffusion path. (Could be an end point)
    Eigen::Vector3d vector_to_path(const DiffusionTransformation &diff_trans, const UnitCellCoord &uccoord);

    /// \brief Determines which site is closest to the diffusion transformation and the vector to take it to the path
    std::pair<UnitCellCoord, Eigen::Vector3d> _path_nearest_neighbor(const DiffusionTransformation &diff_trans) ;

    /// \brief Determines which site is closest to the diffusion transformation
    UnitCellCoord path_nearest_neighbor(const DiffusionTransformation &diff_trans);

    /// \brief Determines the nearest site distance to the diffusion path
    double min_dist_to_path(const DiffusionTransformation &diff_trans);

    /// \brief Determines the vector from the nearest site to the diffusion path in cartesian coordinates
    Eigen::Vector3d min_vector_to_path(const DiffusionTransformation &diff_trans);

    /// \brief Determines whether the atoms moving in the diffusion transformation will collide on a linearly interpolated path
    bool path_collision(const DiffusionTransformation &diff_trans);

  }

  /// \brief Write DiffusionTransformation to JSON object
  jsonParser &to_json(const Kinetics::DiffusionTransformation &trans, jsonParser &json);

  template<>
  struct jsonConstructor<Kinetics::DiffusionTransformation> {

    static Kinetics::DiffusionTransformation from_json(const jsonParser &json, const Structure &prim);
    static Kinetics::DiffusionTransformation from_json(const jsonParser &json, const PrimClex &primclex);
  };

  /// \brief Read from JSON
  void from_json(Kinetics::DiffusionTransformation &trans, const jsonParser &json, const Structure &prim);

  template<>
  struct Printer<Kinetics::DiffusionTransformation> : public PrinterBase {

    typedef Kinetics::DiffusionTransformation Element;
    static const std::string element_name;

    Printer(int _indent_space = 6, char _delim = '\n', COORD_TYPE _mode = INTEGRAL) :
      PrinterBase(_indent_space, _delim, _mode) {}

    void print(const Element &element, Log &out) const;
  };

  template<typename NameIterator>
  bool includes_all(const std::map<AtomSpecies, Index> species_count, NameIterator begin, NameIterator end);

  template<typename NameIterator>
  bool excludes_all(const std::map<AtomSpecies, Index> species_count, NameIterator begin, NameIterator end);

}

#endif
