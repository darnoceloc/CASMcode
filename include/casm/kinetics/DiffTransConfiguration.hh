#ifndef CASM_DiffTransConfiguration
#define CASM_DiffTransConfiguration

#include "casm/misc/Comparisons.hh"
#include "casm/clusterography/ClusterSymCompare.hh"
#include "casm/clex/HasCanonicalForm.hh"
#include "casm/clex/HasSupercell.hh"
#include "casm/clex/Calculable.hh"
#include "casm/clex/Configuration.hh"
#include "casm/clex/ConfigCompare.hh"
#include "casm/kinetics/DiffTransConfigurationTraits.hh"
#include "casm/kinetics/DiffusionTransformation.hh"

namespace CASM {
  class PermuteIterator;

  namespace Kinetics {

    struct DiffTransConfigInsertResult;

    class DiffTransConfiguration :
      public ConfigCanonicalForm<HasSupercell<Comparisons<Calculable<CRTPBase<DiffTransConfiguration>>>>> {

    public:

      /// \brief Constructor
      DiffTransConfiguration(const Configuration &_from_config,
                             const PrimPeriodicDiffTransOrbit &_dtorbit);

      /// \brief Constructor
      DiffTransConfiguration(const Configuration &_from_config,
                             const DiffusionTransformation &_diff_trans);

      /// Construct a DiffTransConfiguration from JSON data
      DiffTransConfiguration(const Supercell &_supercell,
                             const jsonParser &_data);

      /// Construct a DiffTransConfiguration from JSON data
      DiffTransConfiguration(const PrimClex &_primclex,
                             const jsonParser &_data);

      /// \brief Returns the supercell
      const Supercell &supercell() const;

      /// \brief Returns the initial configuration
      const Configuration &from_config() const;

      /// \brief Returns the final configuration
      const Configuration &to_config() const;

      /// \brief Returns the diffusion transformation that is occurring
      const DiffusionTransformation &diff_trans() const;

      /// \brief Compare DiffTransConfiguration
      /// Compares m_diff_trans first then
      /// m_from_config if m_diff_trans are equal
      /// - Comparison is made using the sorted forms
      bool operator<(const DiffTransConfiguration &B) const;

      /// \brief creates a comparison object that can determine if this
      /// is less than another DiffTransConfiguration
      DiffTransConfigCompare less() const;

      /// \brief creates a comparison object that can determine if this
      /// is equal to another DiffTransConfiguration
      DiffTransConfigIsEqual equal_to() const;

      /// \brief sort DiffTransConfiguration in place
      DiffTransConfiguration &sort();

      /// \brief Returns a sorted version of this DiffTransConfiguration
      const DiffTransConfiguration &sorted() const;

      /// \brief Returns true if the DiffTransConfiguration is sorted
      bool is_sorted() const;

      /// \brief applies the symmetry op corresponding to the PermuteIterator to the
      /// DiffTransConfiguration in place
      DiffTransConfiguration &apply_sym(const PermuteIterator &it);

      /// Writes the DiffTransConfiguration to JSON
      jsonParser &to_json(jsonParser &json) const;

      /// Reads the DiffTransConfiguration from JSON
      void from_json(const jsonParser &json, const Supercell &scel);

      /// Reads the DiffTransConfiguration from JSON
      void from_json(const jsonParser &json, const PrimClex &primclex);

      /// Used to store the diff_trans name of this diff_trans_config during enumeration
      void set_orbit_name(const std::string &orbit_name);

      /// Used to store the background config used to generate this diff_trans_config
      void set_bg_configname(const std::string &configname);

      /// Used to store the sub orbit prototype of diff_trans name that this diff_trans_config
      /// contains
      void set_suborbit_ind(const int &suborbit_ind);

      /// \brief This returns an identifier that distinguishes supercell inequivalent but prim equivalent diff_trans
      /// within this diff_trans_config and others with the same orbit_name
      int suborbit_ind() const {
        return m_suborbit_ind;
      }

      /// \brief The name of the prototypical diff_trans that was used to generate this diff_trans_config
      std::string orbit_name() const {
        return m_orbit_name;
      }

      /// \brief The name of the background that was used to generate this diff_trans_config
      std::string bg_configname() const {
        return m_bg_configname;
      }

      /// \brief Sanity check to see if this diff_trans_config has any transformation at all
      bool is_dud() const {
        return from_config() == to_config();
      }

      /// States whether the diffusion transformation is possible with the given Configuration
      bool is_valid() const;

      /// States whether the from specie locations in the diff_trans match the from_config
      bool has_valid_from_occ() const;

      /// The name of the canonical form of the from config
      std::string from_configname() const;

      /// The name of the canonical form of the to config
      std::string to_configname() const;

      /// A permute iterator it such that from_config = copy_apply(it,from_config.canonical_form())
      PermuteIterator from_config_from_canonical() const;

      /// A permute iterator it such that to_config = copy_apply(it,to_config.canonical_form())
      PermuteIterator to_config_from_canonical() const;

      /// \brief Determines if diff_trans is possible with bg_config
      static bool is_valid(const DiffusionTransformation &diff_trans, const Configuration &bg_config);

      /// \brief Determines if diff_trans and bg config are compatible with respect to occupants
      static bool has_valid_from_occ(const DiffusionTransformation &diff_trans, const Configuration &bg_config);

      /// \brief Inserts this into the DiffTransConfigurationDatabase
      DiffTransConfigInsertResult insert() const;

    private:

      friend Named<CRTPBase<DiffTransConfiguration>>;
      /// \brief uses internal fields to generate a name for this diff_trans_config
      std::string generate_name_impl() const;

      void _sort();

      Configuration m_config_A;
      Configuration m_config_B;

      ScelPeriodicDiffTransSymCompare m_sym_compare;

      /// Should always be 'prepared'
      DiffusionTransformation m_diff_trans;

      /// If true, m_config_A is 'from_config()' and m_config_B is 'to_config()',
      /// else it is the reverse
      bool m_from_config_is_A;

      /// fields used to track history of creation for queries and name generation
      std::string m_orbit_name;
      int m_suborbit_ind;
      std::string m_bg_configname;
    };

    struct DiffTransConfigInsertResult {

      typedef DB::DatabaseIterator<DiffTransConfiguration> iterator;

      bool insert_canonical;

      iterator canonical_it;
    };

    /// writes the initial and final positions of atoms to a file
    void write_pos(DiffTransConfiguration const &dtc);

    /// writes the initial and final positions of atoms to a string
    std::string pos_string(DiffTransConfiguration const &dtc);

    /// \brief prints this DiffTransConfiguration
    std::ostream &operator<<(std::ostream &sout, const DiffTransConfiguration &dtc) ;

    /// \brief returns a copy of bg_config with sites altered such that diff_trans can be placed as is
    Configuration make_attachable(const DiffusionTransformation &diff_trans, const Configuration &bg_config);

    /// \brief Returns correlations using 'clexulator'.
    Eigen::VectorXd correlations(const DiffTransConfiguration &dtc, Clexulator &clexulator);

    /// \brief Indicates whether there is a valid kra for DiffTransConfiguration
    bool has_kra(const DiffTransConfiguration &dtc);

    /// \brief Returns kra for DiffTransConfiguration
    double kra(const DiffTransConfiguration &dtc);

    /// \brief Returns the distance to furthest perturbation of the background config from diffusion hop
    double max_perturb_rad(const DiffTransConfiguration &dtc);

    /// \brief Returns the distance to closest perturbation of the background config from diffusion hop
    double min_perturb_rad(const DiffTransConfiguration &dtc);

  }

  template<>
  struct jsonConstructor<Kinetics::DiffTransConfiguration> {

    Kinetics::DiffTransConfiguration from_json(
      const jsonParser &json,
      const PrimClex &primclex);

    Kinetics::DiffTransConfiguration from_json(
      const jsonParser &json,
      const Supercell &scel);
  };
}

#endif