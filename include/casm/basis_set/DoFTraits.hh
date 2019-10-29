#ifndef CASM_DoFTraits
#define CASM_DoFTraits

#include "casm/basis_set/DoF.hh"
#include "casm/basis_set/FunctionVisitor.hh"
#include "casm/symmetry/OrbitDecl.hh"
#include "casm/clusterography/ClusterDecl.hh"
#include "casm/misc/ParsingDictionary.hh"


namespace CASM {
  namespace xtal {
    class Site;
    class Structure;
    template<typename CoordType>
    class BasicStructure;
    class SimpleStructure;
    class UnitCellCoord;
    class SymOp;
  }
  using xtal::Site;
  using xtal::Structure;
  using xtal::BasicStructure;
  using xtal::SimpleStructure;
  using xtal::UnitCellCoord;

  class jsonParser;
  class PrimNeighborList;
  class BasisSet;

  class ConfigDoF;

  namespace DoFType {
    class Traits;
    struct ParamAllocation;

    Traits const &traits(std::string const &dof_key);

    BasicTraits const &basic_traits(std::string const &dof_key);

    //DoF_impl::OccupationDoFTraits occupation();

    /// \brief Collection of all the traits specific to a DoF type

    class Traits : public BasicTraits {
    public:
      Traits(std::string const &_type_name,
             std::vector<std::string> const &_std_var_names,
             DOF_MODE _mode,
             bool _requires_site_basis,
             bool _unit_length) :
        BasicTraits(_type_name,
                    _std_var_names,
                    _mode,
                    _requires_site_basis,
                    _unit_length) {

      }

      /// \brief Allow destruction through base pointer
      virtual ~Traits() {}

      /// \brief Construct the site basis (if DOF_MODE is LOCAL) for a DoF, given its site
      virtual std::vector<BasisSet> construct_site_bases(Structure const &_prim,
                                                         std::vector<Orbit<PrimPeriodicSymCompare<IntegralCluster> > > &_asym_unit,
                                                         jsonParser const &_bspecs) const = 0;


      /// \brief Populate @param _in from JSON
      virtual void from_json(DoFSet &_in, jsonParser const &_json) const { }

      /// \brief Output @param _in to JSON
      virtual void to_json(DoFSet const &_out, jsonParser &_json) const;

      /// \brief Generate a symmetry representation for the supporting vector space
      virtual Eigen::MatrixXd symop_to_matrix(xtal::SymOp const &op) const = 0;

      /// \brief Transforms SimpleSructure @param _struc by applying DoF values contained in @param _dof in a type-specific way
      virtual void apply_dof(ConfigDoF const &_dof, BasicStructure<Site> const &_reference, SimpleStructure &_struc) const;

      /// \brief Serialize type-specific DoF values from ConfigDoF
      virtual jsonParser dof_to_json(ConfigDoF const &_dof, BasicStructure<Site> const &_reference) const;

      // ** The following functionality is utilized for controlling clexulator printing. It only needs to be overridden in special cases **

      /// \brief
      virtual std::vector<std::unique_ptr<FunctionVisitor> > site_function_visitors(std::string const &nlist_specifier = "%n") const;

      virtual std::vector<std::unique_ptr<FunctionVisitor> > clust_function_visitors() const;

      virtual std::string site_basis_description(BasisSet site_bset, Site site) const;

      virtual std::vector<ParamAllocation> param_pack_allocation(Structure const &_prim,
                                                                 std::vector<BasisSet> const &_bases) const;

      virtual std::string clexulator_constructor_string(Structure const &_prim,
                                                        std::vector<BasisSet> const &site_bases,
                                                        std::string const &indent) const;

      virtual std::string clexulator_point_prepare_string(Structure const &_prim,
                                                          std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                          PrimNeighborList &_nlist,
                                                          std::vector<BasisSet> const &site_bases,
                                                          std::string const &indent) const;

      virtual std::string clexulator_global_prepare_string(Structure const &_prim,
                                                           std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                           PrimNeighborList &_nlist,
                                                           std::vector<BasisSet> const &site_bases,
                                                           std::string const &indent) const;

      virtual std::string clexulator_member_declarations_string(Structure const &_prim,
                                                                std::vector<BasisSet> const &site_bases,
                                                                std::string const &indent) const;

      virtual std::string clexulator_private_method_declarations_string(Structure const &_prim,
                                                                        std::vector<BasisSet> const &site_bases,
                                                                        std::string const &indent) const;

      virtual std::string clexulator_public_method_declarations_string(Structure const &_prim,
                                                                       std::vector<BasisSet> const &site_bases,
                                                                       std::string const &indent) const;

      virtual std::string clexulator_private_method_definitions_string(Structure const &_prim,
                                                                       std::vector<BasisSet> const &site_bases,
                                                                       std::string const &indent) const;

      virtual std::string clexulator_public_method_definitions_string(Structure const &_prim,
                                                                      std::vector<BasisSet> const &site_bases,
                                                                      std::string const &indent) const;

      /// \brief non-virtual method to obtain copy through Traits pointer
      std::unique_ptr<Traits> clone() const {
        return std::unique_ptr<Traits>(static_cast<Traits *>(_clone()));
      }
    };


    struct ParamAllocation {
    public:
      ParamAllocation(std::string const &_param_name, Index _param_dim, Index _num_param, bool _independent) :
        param_name(_param_name),
        param_dim(_param_dim),
        num_param(_num_param),
        independent(_independent) {}


      const std::string param_name;
      const Index param_dim;
      const Index num_param;
      const bool independent;

    };
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    /// \brief  Parsing dictionary for obtaining the correct BasicTraits given a name
    using TraitsDictionary = ParsingDictionary<BasicTraits>;

    /// This will eventually be managed by ProjectSettings
    //TraitsDictionary const &traits_dict();


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


    inline
    Traits const &traits(std::string const &dof_key) {
      return static_cast<Traits const &>(DoF::traits(dof_key));
    }

    inline
    BasicTraits const &basic_traits(std::string const &dof_key) {
      return DoF::traits(dof_key);
    }

  }

  template<>
  DoFType::TraitsDictionary make_parsing_dictionary<DoF::BasicTraits>();


}
#endif
