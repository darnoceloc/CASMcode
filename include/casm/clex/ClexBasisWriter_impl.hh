#ifndef CASM_ClexBasisWriter_impl
#define CASM_ClexBasisWriter_impl

#include "casm/symmetry/Orbit_impl.hh"
#include "casm/clex/ClexBasisWriter.hh"
#include "casm/clex/OrbitFunctionTraits.hh"
#include "casm/clex/NeighborList.hh"
#include "casm/clusterography/IntegralCluster_impl.hh"
#include "casm/basis_set/DoFTraits.hh"
#include "casm/basis_set/FunctionVisitor.hh"
#include "casm/app/AppIO.hh"
#include "casm/crystallography/Structure.hh"

namespace CASM {
  //*******************************************************************************************
  /// \brief Print clexulator
  template<typename OrbitType>
  void ClexBasisWriter::print_clexulator(std::string class_name,
                                         ClexBasis const &clex,
                                         std::vector<OrbitType > const &_tree,
                                         PrimNeighborList &_nlist,
                                         std::vector<UnitCellCoord> const &_flower_pivots,
                                         std::ostream &stream,
                                         double xtal_tol) {

    enum FuncStringType {func_declaration = 0, func_definition = 1};

    Index N_sublat = clex.n_sublat();

    Index N_corr = clex.n_functions();

    std::map<UnitCellCoord, std::set<UnitCellCoord> > nhood = ClexBasisWriter_impl::dependency_neighborhood(_tree.begin(), _tree.end());

    Index N_branch = nhood.size();

    for(auto const &nbor : nhood)
      N_branch = max(_nlist.neighbor_index(nbor.first) + 1, N_branch);

    std::stringstream bfunc_imp_stream, bfunc_def_stream;

    std::string indent(2, ' ');

    std::stringstream parampack_stream;
    print_param_pack(class_name,
                     clex,
                     _tree,
                     _nlist,
                     _flower_pivots,
                     parampack_stream,
                     indent);


    std::string uclass_name;
    for(Index i = 0; i < class_name.size(); i++)
      uclass_name.push_back(std::toupper(class_name[i]));


    std::string private_declarations = ClexBasisWriter_impl::clexulator_member_declarations(class_name, clex, _orbit_func_traits(), nhood, indent + "  ");

    private_declarations += ClexBasisWriter_impl::clexulator_private_method_declarations(class_name, clex, indent + "  ");

    std::string public_declarations = ClexBasisWriter_impl::clexulator_public_method_declarations(class_name, clex, indent + "  ");


    //linear function index
    Index lf = 0;

    std::vector<std::unique_ptr<FunctionVisitor> > const &visitors(_clust_function_visitors());
    //std::cout << "Initialized " << visitors.size() << " visitors \n";

    std::vector<std::string> orbit_method_names(N_corr, "zero_func");

    std::vector<std::vector<std::string> > flower_method_names(N_branch, std::vector<std::string>(N_corr, "zero_func"));

    //this is very configuration-centric
    std::vector<std::vector<std::string> > dflower_method_names(N_branch, std::vector<std::string>(N_corr, "zero_func"));

    // temporary storage for formula
    std::vector<std::string> formulae, tformulae;

    bool make_newline(false);

    //loop over orbits
    for(Index no = 0; no < _tree.size(); no++) {

      if(_tree[no].prototype().size() == 0)
        bfunc_imp_stream <<
                         indent << "// Basis functions for empty cluster:\n";
      else {
        bfunc_imp_stream <<
                         indent << "/**** Basis functions for orbit " << no << "****\n";
        ProtoSitesPrinter().print(_tree[no].prototype(), bfunc_imp_stream);
        bfunc_imp_stream << indent << "****/\n";
      }

      auto orbit_method_namer = [lf, no, &orbit_method_names](Index nb, Index nf)->std::string{
        orbit_method_names[lf + nf] = "eval_bfunc_" + std::to_string(no) + "_" + std::to_string(nf);
        return orbit_method_names[lf + nf];
      };

      std::tuple<std::string, std::string> orbit_functions = ClexBasisWriter_impl::clexulator_orbit_function_strings(class_name,
                                                             clex.bset_orbit(no),
                                                             _tree[no],
                                                             orbit_method_namer,
                                                             _nlist,
                                                             visitors,
                                                             indent);

      bfunc_def_stream << std::get<func_declaration>(orbit_functions);
      bfunc_imp_stream << std::get<func_definition>(orbit_functions);

      auto flower_method_namer = [lf, no, &flower_method_names](Index nb, Index nf)->std::string{
        flower_method_names[nb][lf + nf] = "site_eval_bfunc_"  + std::to_string(no) + "_" + std::to_string(nf) + "_at_" + std::to_string(nb);
        return flower_method_names[nb][lf + nf];
      };

      std::tuple<std::string, std::string> flower_functions = ClexBasisWriter_impl::clexulator_flower_function_strings(class_name,
                                                              clex.bset_orbit(no),
                                                              _tree[no],
                                                              flower_method_namer,
                                                              nhood,
                                                              _nlist,
                                                              visitors,
                                                              indent);

      bfunc_def_stream << std::get<func_declaration>(flower_functions);
      bfunc_imp_stream << std::get<func_definition>(flower_functions);

      auto dflower_method_namer = [lf, no, &dflower_method_names](Index nb, Index nf)->std::string{
        dflower_method_names[nb][lf + nf] = "site_deval_bfunc_"  + std::to_string(no) + "_" + std::to_string(nf) + "_at_" + std::to_string(nb);
        return dflower_method_names[nb][lf + nf];
      };

      // Hard-coded for occupation currently... this should be encoded in DoFTraits instead.
      std::string dof_key("occ");
      OccFuncLabeler site_prefactor_labeler("(m_occ_func_%b_%f[occ_f] - m_occ_func_%b_%f[occ_i])");

      auto site_bases_iter = clex.site_bases().find(dof_key);
      if(site_bases_iter == clex.site_bases().end())
        throw std::runtime_error(std::string("Unable to look up site basis ") + dof_key + " during clexulator printing.\n");

      std::tuple<std::string, std::string> dflower_functions = ClexBasisWriter_impl::clexulator_dflower_function_strings(class_name,
                                                               clex.bset_orbit(no),
                                                               site_bases_iter->second,
                                                               _tree[no],
                                                               dflower_method_namer,
                                                               nhood,
                                                               _nlist,
                                                               visitors,
                                                               site_prefactor_labeler,
                                                               indent);


      bfunc_def_stream << std::get<func_declaration>(dflower_functions);
      bfunc_imp_stream << std::get<func_definition>(dflower_functions);

      lf += clex.bset_orbit(no)[0].size();
    }//Finished writing method definitions and definitions for basis functions

    std::string constructor_definition = ClexBasisWriter_impl::clexulator_constructor_definition(class_name,
                                         clex,
                                         _tree,
                                         nhood,
                                         _nlist,
                                         orbit_method_names,
                                         flower_method_names,
                                         dflower_method_names,
                                         indent);

    std::string interface_declaration = ClexBasisWriter_impl::clexulator_interface_declaration(class_name, clex, indent);

    std::string prepare_methods_definition = ClexBasisWriter_impl::clexulator_point_prepare_definition(class_name,
                                             clex,
                                             _tree,
                                             _orbit_func_traits(),
                                             nhood,
                                             _nlist,
                                             indent);

    prepare_methods_definition += ClexBasisWriter_impl::clexulator_global_prepare_definition(class_name,
                                  clex,
                                  _tree,
                                  _orbit_func_traits(),
                                  nhood,
                                  _nlist,
                                  indent);

    jsonParser json_prim;
    write_prim(clex.prim(), json_prim, FRAC);

    // PUT EVERYTHING TOGETHER
    stream
        << "#include <cstddef>\n"
        << "#include \"casm/clex/Clexulator.hh\"\n"
        << "\n\n\n"

        << "/****** PROJECT SPECIFICATIONS ******\n\n"

        << "         ****** prim.json ******\n\n"
        << json_prim << "\n\n"
        << "        ****** bspecs.json ******\n\n"
        << clex.bspecs() << "\n\n"
        << "**/\n\n\n"


        << "/// \\brief Returns a Clexulator_impl::Base* owning a " << class_name << "\n"
        << "extern \"C\" CASM::Clexulator_impl::Base* make_" + class_name << "();\n\n"

        << "namespace CASM {\n\n"


        << "/****** GENERATED CLEXPARAMPACK DEFINITION ******/\n\n"

        << parampack_stream.str() << "\n\n"

        << "/****** GENERATED CLEXULATOR DEFINITION ******/\n\n"

        << indent << "class " << class_name << " : public Clexulator_impl::Base {\n\n"

        << indent << "public:\n\n"
        << public_declarations << "\n"
        << bfunc_def_stream.str() << "\n"

        << indent << "private:\n\n"
        << private_declarations << "\n"

        << indent << "};\n\n" // close class definition

        << indent

        << "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n"

        << constructor_definition << "\n"
        << interface_declaration << "\n"
        << prepare_methods_definition << "\n"
        << bfunc_imp_stream.str()
        << "}\n\n\n"      // close namespace

        << "extern \"C\" {\n"
        << indent << "/// \\brief Returns a Clexulator_impl::Base* owning a " << class_name << "\n"
        << indent << "CASM::Clexulator_impl::Base* make_" + class_name << "() {\n"
        << indent << "  return new CASM::" + class_name + "();\n"
        << indent << "}\n\n"
        << "}\n"

        << "\n";
    // EOF

    return;
  }

  template<typename OrbitType>
  void ClexBasisWriter::print_param_pack(std::string clexclass_name,
                                         ClexBasis const &clex,
                                         std::vector<OrbitType > const &_tree,
                                         PrimNeighborList &_nlist,
                                         std::vector<UnitCellCoord> const &_flower_pivots,
                                         std::ostream &stream,
                                         std::string const &_indent) const {

    std::string ppclass_name = clexclass_name + "ParamPack";


    stream <<
           _indent << "typedef BasicClexParamPack " << ppclass_name << ";\n";


  }
  namespace ClexBasisWriter_impl {

    template<typename OrbitType>
    std::tuple<std::string, std::string> clexulator_orbit_function_strings(std::string const &class_name,
                                                                           ClexBasis::BSetOrbit const &_bset_orbit,
                                                                           OrbitType const &_clust_orbit,
                                                                           std::function<std::string(Index, Index)> method_namer,
                                                                           PrimNeighborList &_nlist,
                                                                           std::vector<std::unique_ptr<FunctionVisitor> > const &visitors,
                                                                           std::string const &indent) {

      std::stringstream bfunc_def_stream;
      std::stringstream bfunc_imp_stream;


      std::vector<std::string>formulae = orbit_function_cpp_strings(_bset_orbit, _clust_orbit, _nlist, visitors);
      std::string method_name;

      bool make_newline = false;
      for(Index nf = 0; nf < formulae.size(); nf++) {
        if(!formulae[nf].size())
          continue;
        make_newline = true;

        method_name = method_namer(0, nf);
        bfunc_def_stream <<
                         indent << "  double " << method_name << "() const;\n";

        bfunc_imp_stream <<
                         indent << "double " << class_name << "::" << method_name << "() const{\n" <<
                         indent << "  return " << formulae[nf] << ";\n" <<
                         indent << "}\n";
      }
      if(make_newline) {
        bfunc_imp_stream << '\n';
        bfunc_def_stream << '\n';
      }
      return std::tuple<std::string, std::string>(bfunc_def_stream.str(), bfunc_imp_stream.str());
    }

    //*******************************************************************************************
    template<typename OrbitType>
    std::tuple<std::string, std::string> clexulator_flower_function_strings(std::string const &class_name,
                                                                            ClexBasis::BSetOrbit const &_bset_orbit,
                                                                            OrbitType const &_clust_orbit,
                                                                            std::function<std::string(Index, Index)> method_namer,
                                                                            std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                                            PrimNeighborList &_nlist,
                                                                            std::vector<std::unique_ptr<FunctionVisitor> > const &visitors,
                                                                            std::string const &indent) {
      std::stringstream bfunc_def_stream, bfunc_imp_stream;
      std::string method_name;
      // loop over flowers (i.e., basis sites of prim)
      bool make_newline = false;

      for(auto const &nbor : _nhood) {
        make_newline = false;

        std::cout << "Entering flower functions...\n";
        std::vector<std::string> formulae = flower_function_cpp_strings(_bset_orbit,
                                                                        CASM_TMP::UnaryIdentity<BasisSet>(),
                                                                        _clust_orbit,
                                                                        _nhood,
                                                                        _nlist,
                                                                        visitors,
                                                                        nbor.first);

        Index nbor_ind = _nlist.neighbor_index(nbor.first);

        for(Index nf = 0; nf < formulae.size(); nf++) {
          if(!formulae[nf].size())
            continue;
          make_newline = true;

          method_name = method_namer(nbor_ind, nf);
          bfunc_def_stream <<
                           indent << "  double " << method_name << "() const;\n";

          bfunc_imp_stream <<
                           indent << "double " << class_name << "::" << method_name << "() const{\n" <<
                           indent << "  return " << formulae[nf] << ";\n" <<
                           indent << "}\n";

        }
      }
      if(make_newline) {
        bfunc_imp_stream << '\n';
        bfunc_def_stream << '\n';
      }

      return std::tuple<std::string, std::string>(bfunc_def_stream.str(), bfunc_imp_stream.str());
    }


    //*******************************************************************************************

    template<typename OrbitType>
    std::tuple<std::string, std::string> clexulator_dflower_function_strings(std::string const &class_name,
                                                                             ClexBasis::BSetOrbit const &_bset_orbit,
                                                                             ClexBasis::BSetOrbit const &_site_bases,
                                                                             OrbitType const &_clust_orbit,
                                                                             std::function<std::string(Index, Index)> method_namer,
                                                                             std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                                             PrimNeighborList &_nlist,
                                                                             std::vector<std::unique_ptr<FunctionVisitor> > const &visitors,
                                                                             FunctionVisitor const &_site_func_labeler,
                                                                             std::string const &indent) {
      std::stringstream bfunc_def_stream, bfunc_imp_stream;
      bool make_newline = false;

      // loop over flowers (i.e., basis sites of prim)
      for(auto const &nbor : _nhood) {
        std::vector<std::string> formulae(_bset_orbit[0].size());

        Index nbor_ind = _nlist.neighbor_index(nbor.first);
        Index sublat_ind = nbor.first.sublat();

        // loop over site basis functions
        BasisSet site_basis(_site_bases[sublat_ind]);
        site_basis.set_dof_IDs(std::vector<Index>(1, nbor_ind));

        site_basis.accept(_site_func_labeler);
        for(Index nsbf = 0; nsbf < site_basis.size(); nsbf++) {
          Function const *func_ptr = site_basis[nsbf];

          auto get_quotient_bset = [func_ptr](BasisSet const & bset)->BasisSet{
            return bset.poly_quotient_set(func_ptr);
          };

          std::cout << "Entering dflower functions...\n";
          std::vector<std::string> tformulae = flower_function_cpp_strings(_bset_orbit,
                                                                           get_quotient_bset,
                                                                           _clust_orbit,
                                                                           _nhood,
                                                                           _nlist,
                                                                           visitors,
                                                                           nbor.first);


          for(Index nf = 0; nf < tformulae.size(); nf++) {
            if(!tformulae[nf].size())
              continue;

            if(formulae[nf].size())
              formulae[nf] += " + ";

            formulae[nf] += site_basis[nsbf]->formula();

            if(tformulae[nf] == "1" || tformulae[nf] == "(1)")
              continue;

            formulae[nf] += "*";
            formulae[nf] += tformulae[nf];
            //formulae[nf] += ")";
          }
        }

        std::string method_name;
        make_newline = false;
        for(Index nf = 0; nf < formulae.size(); nf++) {
          if(!formulae[nf].size()) {
            continue;
          }
          make_newline = true;

          method_name = method_namer(nbor_ind, nf);

          bfunc_def_stream <<
                           indent << "  double " << method_name << "(int occ_i, int occ_f) const;\n";

          bfunc_imp_stream <<
                           indent << "double " << class_name << "::" << method_name << "(int occ_i, int occ_f) const{\n" <<
                           indent << "  return " << formulae[nf] << ";\n" <<
                           indent << "}\n";
        }
        if(make_newline) {
          bfunc_imp_stream << '\n';
          bfunc_def_stream << '\n';
        }
        make_newline = false;
        // \End Configuration specific part
      }//\End loop over flowers
      return std::tuple<std::string, std::string>(bfunc_def_stream.str(), bfunc_imp_stream.str());
    }

    //*******************************************************************************************

    // Divide by multiplicity. Same result as evaluating correlations via orbitree.
    template<typename OrbitType>
    std::vector<std::string> orbit_function_cpp_strings(ClexBasis::BSetOrbit _bset_orbit, // used as temporary
                                                        OrbitType const &_clust_orbit,
                                                        PrimNeighborList &_nlist,
                                                        std::vector<std::unique_ptr<FunctionVisitor> > const &visitors) {
      std::string prefix, suffix;
      std::vector<std::string> formulae(_bset_orbit[0].size(), std::string());
      if(_clust_orbit.size() > 1) {
        prefix = "(";
        suffix = ")/" + std::to_string(_clust_orbit.size()) + ".";
      }

      for(Index ne = 0; ne < _bset_orbit.size(); ne++) {
        std::vector<PrimNeighborList::Scalar> nbor_IDs =
          _nlist.neighbor_indices(_clust_orbit[ne].elements().begin(),
                                  _clust_orbit[ne].elements().end());
        _bset_orbit[ne].set_dof_IDs(std::vector<Index>(nbor_IDs.begin(), nbor_IDs.end()));
        for(Index nl = 0; nl < visitors.size(); nl++)
          _bset_orbit[ne].accept(*visitors[nl]);
      }

      for(Index nf = 0; nf < _bset_orbit[0].size(); nf++) {
        for(Index ne = 0; ne < _bset_orbit.size(); ne++) {
          if(!_bset_orbit[ne][nf] || (_bset_orbit[ne][nf]->is_zero()))
            continue;

          if(formulae[nf].empty())
            formulae[nf] += prefix;
          else if((_bset_orbit[ne][nf]->formula())[0] != '-' && (_bset_orbit[ne][nf]->formula())[0] != '+')
            formulae[nf] += " + ";

          formulae[nf] += _bset_orbit[ne][nf]->formula();
        }

        if(!formulae[nf].empty())
          formulae[nf] += suffix;
      }
      return formulae;
    }

    //*******************************************************************************************
    /// nlist_index is the index of the basis site in the neighbor list
    template<typename OrbitType>
    std::vector<std::string> flower_function_cpp_strings(ClexBasis::BSetOrbit _bset_orbit, // used as temporary
                                                         std::function<BasisSet(BasisSet const &)> _bset_transform,
                                                         OrbitType const &_clust_orbit,
                                                         std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                         PrimNeighborList &_nlist,
                                                         std::vector<std::unique_ptr<FunctionVisitor> > const &visitors,
                                                         UnitCellCoord const &nbor) {


      std::vector<std::string> formulae;

      Index nbor_ind = _nlist.neighbor_index(nbor);

      std::string prefix, suffix;

      std::set<UnitCellCoord> trans_set;

      //Find ucc's that might be translationally equivalent to current neighbor
      for(IntegralCluster const &equiv : _clust_orbit) {
        for(UnitCellCoord const &site : equiv.elements()) {
          if(site.sublat() == nbor.sublat()) {
            trans_set.insert(site);
          }
        }
      }

      std::cout << "Trans_set:\n";
      for(UnitCellCoord const &site : trans_set)
        std::cout << "        " << site << "\n";

      std::cout << "\n";

      std::set<UnitCellCoord> equiv_ucc = ClexBasisWriter_impl::equiv_ucc(trans_set.begin(),
                                                                          trans_set.end(),
                                                                          nbor,
                                                                          _clust_orbit.sym_compare());


      std::cout << "equiv_ucc:\n";
      for(UnitCellCoord const &site : equiv_ucc)
        std::cout << "        " << site << "\n";

      std::cout << "\n";
      //normalize by multiplicity (by convention)
      if(_clust_orbit.size() > 1) {
        prefix = "(";
        suffix = ")/" + std::to_string(_clust_orbit.size()) + ".";
      }

      // loop over equivalent clusters
      for(Index ne = 0; ne < _clust_orbit.size(); ne++) {
        if(formulae.empty())
          formulae.resize(_bset_orbit[ne].size());
        std::cout << "ne = " << ne << std::endl;

        /// For each equivalent ucc that is in the equivalent cluster, translate the cluster so that the site coincides with nbor
        for(UnitCellCoord const &trans : equiv_ucc) {
          if(!contains(_clust_orbit[ne].elements(), trans))
            continue;

          typename OrbitType::Element trans_clust = _clust_orbit[ne] - (trans.unitcell() - nbor.unitcell());


          std::vector<PrimNeighborList::Scalar> nbor_IDs =
            _nlist.neighbor_indices(trans_clust.elements().begin(),
                                    trans_clust.elements().end());
          std::cout << "new nbor_IDs: " << nbor_IDs << std::endl;
          _bset_orbit[ne].set_dof_IDs(std::vector<Index>(nbor_IDs.begin(), nbor_IDs.end()));

          BasisSet transformed_bset(_bset_transform(_bset_orbit[ne]));
          for(Index nl = 0; nl < visitors.size(); nl++)
            transformed_bset.accept(*visitors[nl]);

          std::cout << "Transformed BSet size: " << transformed_bset.size() << std::endl;
          for(Index nf = 0; nf < transformed_bset.size(); nf++) {
            std::cout << "Formula before transform: " << _bset_orbit[ne][nf]->formula() << std::endl;
            std::cout << "Transformed BSet " << nf << " of " << transformed_bset.size() << std::endl;
            std::cout << "Formula after transform: " << (transformed_bset[nf] ? transformed_bset[nf]->formula() : " NULL ") << std::endl;
            if(!transformed_bset[nf] || (transformed_bset[nf]->is_zero())) {
              std::cout << "DISCARDING\n\n";
              continue;

            }
            std::cout << "KEEPING\n\n";

            if(formulae[nf].empty())
              formulae[nf] += prefix;
            else if((transformed_bset[nf]->formula())[0] != '-' && (transformed_bset[nf]->formula())[0] != '+')
              formulae[nf] += " + ";

            formulae[nf] += transformed_bset[nf]->formula();

          }
        }
      }

      for(Index nf = 0; nf < formulae.size(); nf++) {
        if(!formulae[nf].empty())
          formulae[nf] += suffix;
        std::cout << "Formula [" << nbor_ind  << "] " << nf << ":  " << formulae[nf] << "\n";
      }

      return formulae;

    }

    //*******************************************************************************************
    template<typename OrbitType>
    void print_proto_clust_funcs(ClexBasis const &clex,
                                 std::ostream &out,
                                 std::vector<OrbitType > const &_tree) {
      //Prints out all prototype clusters (CLUST file)

      out << "COORD_MODE = " << COORD_MODE::NAME() << std::endl << std::endl;

      auto const &site_bases(clex.site_bases());

      out.flags(std::ios::showpoint | std::ios::fixed | std::ios::left);
      out.precision(5);
      auto it = site_bases.begin(), end_it = site_bases.end();
      for(; it != end_it; ++it) {
        out << "Basis site definitions for DoF " << it->first << ".\n";

        for(Index nb = 0; nb < (it->second).size(); ++nb) {
          out << "  Basis site " << nb + 1 << ":\n"
              << "  ";
          clex.prim().basis()[nb].print(out);
          out << "\n";
          out << DoFType::traits(it->first).site_basis_description((it->second)[nb], clex.prim().basis()[nb]);
        }
      }

      out << "\n\n";
      Index nf = 0;
      for(Index i = 0; i < _tree.size(); i++) {
        if(i == 0 || _tree[i].prototype().size() != _tree[i - 1].prototype().size())
          out << "** " << _tree[i].prototype().size() << "-site clusters ** \n" << std::flush;

        out << "      ** Orbit " << i + 1 << " of " << _tree.size() << " **"
            << "  Points: " << _tree[i].prototype().size()
            << "  Mult: " << _tree[i].size()
            << "  MinLength: " << _tree[i].prototype().min_length()
            << "  MaxLength: " << _tree[i].prototype().max_length()
            << '\n' << std::flush;

        out << "            " << "Prototype" << " of " << _tree[i].size() << " Equivalent Clusters in Orbit " << i << '\n' << std::flush;
        print_clust_basis(out,
                          clex.bset_orbit(i)[0],
                          _tree[i].prototype(),
                          nf, 8, '\n');

        nf += clex.bset_orbit(i)[0].size();
        out << "\n\n" << std::flush;

        if(_tree[i].prototype().size() != 0) out << '\n' << std::flush;
      }
    }

    //*******************************************************************************************

    // keys of result are guaranteed to be in canonical translation unit
    template<typename OrbitIterType>
    std::map<UnitCellCoord, std::set<UnitCellCoord> > dependency_neighborhood(OrbitIterType begin,
                                                                              OrbitIterType end) {

      std::map<UnitCellCoord, std::set<UnitCellCoord> > result;

      if(begin == end)
        return result;

      typedef IntegralCluster cluster_type;
      typedef typename OrbitIterType::value_type orbit_type;

      UnitCellCoord const *ucc_ptr(nullptr);
      OrbitIterType begin2 = begin;
      for(; begin2 != end; ++begin2) {

        auto const &orbit = *begin2;
        for(auto const &equiv : orbit) {
          for(UnitCellCoord const &ucc : equiv) {
            ucc_ptr = &ucc;
            break;
          }
          if(ucc_ptr)
            break;
        }
        if(ucc_ptr)
          break;
      }
      if(!ucc_ptr)
        return result;

      SymGroup identity_group((ucc_ptr->unit()).factor_group().begin(), ((ucc_ptr->unit()).factor_group().begin()) + 1);
      orbit_type empty_orbit(cluster_type(ucc_ptr->unit()), identity_group, begin->sym_compare());


      // Loop over each site in each cluster of each orbit
      for(; begin != end; ++begin) {

        auto const &orbit = *begin;
        for(auto const &equiv : orbit) {
          for(UnitCellCoord const &ucc : equiv) {
            // create a test cluster from prototype
            cluster_type test(empty_orbit.prototype());

            // add the new site
            test.elements().push_back(ucc);
            test = orbit.sym_compare().prepare(test);

            UnitCell trans = test.element(0).unitcell() - ucc.unitcell();
            for(UnitCellCoord const &ucc2 : equiv) {
              result[test.element(0)].insert(ucc2 + trans);
            }
          }
        }
      }
      return result;

    }

    //*******************************************************************************************


    template<typename UCCIterType, typename IntegralClusterSymCompareType>
    std::set<UnitCellCoord> equiv_ucc(UCCIterType begin,
                                      UCCIterType end,
                                      UnitCellCoord const &pivot,
                                      IntegralClusterSymCompareType const &sym_compare) {
      std::set<UnitCellCoord>  result;

      typedef IntegralCluster cluster_type;
      typedef Orbit<cluster_type, IntegralClusterSymCompareType> orbit_type;


      if(begin == end)
        return result;


      SymGroup identity_group((begin->unit()).factor_group().begin(), ((begin->unit()).factor_group().begin()) + 1);
      orbit_type empty_orbit(cluster_type(begin->unit()), identity_group, sym_compare);

      cluster_type pclust(empty_orbit.prototype());
      pclust.elements().push_back(pivot);
      pclust = sym_compare.prepare(pclust);
      //std::cout << "pclust elements: \n" << pclust.elements() << "\n\n";
      // by looping over each site in the grid,
      for(; begin != end; ++begin) {

        // create a test cluster from prototype
        cluster_type test(empty_orbit.prototype());

        // add the new site
        test.elements().push_back(*begin);
        //std::cout << "Before prepare: " << test.element(0) << std::endl;
        test = sym_compare.prepare(test);
        //std::cout << "After prepare: " << test.element(0) << std::endl << std::endl;
        //std::cout << "test elements: \n" << test.elements() << "\n";


        if(sym_compare.equal(test, pclust)) {
          result.insert(*begin);

        }

      }

      return result;

    }

    //*******************************************************************************************

    template<typename OrbitType>
    std::string clexulator_constructor_definition(std::string const &class_name,
                                                  ClexBasis const &clex,
                                                  std::vector<OrbitType > const &_tree,
                                                  std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                  PrimNeighborList &_nlist,
                                                  std::vector<std::string> const &orbit_method_names,
                                                  std::vector< std::vector<std::string> > const &flower_method_names,
                                                  std::vector< std::vector<std::string> > const &dflower_method_names,
                                                  std::string const &indent) {

      Index N_corr = clex.n_functions();

      Index N_branch = _nhood.size();

      for(auto const &nbor : _nhood)
        N_branch = max(_nlist.neighbor_index(nbor.first) + 1, N_branch);

      std::stringstream ss;
      // Write constructor
      ss <<
         indent << class_name << "::" << class_name << "() :\n" <<
         indent << "  Clexulator_impl::Base(" << N_branch << ", " << N_corr << ") {\n";

      {
        auto it(clex.site_bases().begin()), end_it(clex.site_bases().end());
        for(; it != end_it; ++it) {
          DoFType::traits(it->first).clexulator_constructor_string(clex.prim(), it->second, indent + "  ");
        }
      }

      for(Index nf = 0; nf < orbit_method_names.size(); nf++) {
        ss <<
           indent << "  m_orbit_func_table[" << nf << "] = &" << class_name << "::" << orbit_method_names[nf] << ";\n";
      }
      ss << "\n\n";

      for(Index nb = 0; nb < flower_method_names.size(); nb++) {
        for(Index nf = 0; nf < flower_method_names[nb].size(); nf++) {
          ss <<
             indent << "  m_flower_func_table[" << nb << "][" << nf << "] = &" << class_name << "::" << flower_method_names[nb][nf] << ";\n";
        }
        ss << "\n\n";
      }

      for(Index nb = 0; nb < dflower_method_names.size(); nb++) {
        for(Index nf = 0; nf < dflower_method_names[nb].size(); nf++) {
          ss <<
             indent << "  m_delta_func_table[" << nb << "][" << nf << "] = &" << class_name << "::" << dflower_method_names[nb][nf] << ";\n";
        }
        ss << "\n\n";
      }



      // Write weight matrix used for the neighbor list
      PrimNeighborList::Matrix3Type W = _nlist.weight_matrix();
      ss << indent << "  m_weight_matrix.row(0) << " << W(0, 0) << ", " << W(0, 1) << ", " << W(0, 2) << ";\n";
      ss << indent << "  m_weight_matrix.row(1) << " << W(1, 0) << ", " << W(1, 1) << ", " << W(1, 2) << ";\n";
      ss << indent << "  m_weight_matrix.row(2) << " << W(2, 0) << ", " << W(2, 1) << ", " << W(2, 2) << ";\n\n";

      // Write neighborhood of UnitCellCoord
      // expand the _nlist to contain 'global_orbitree' (all that is needed for now)
      std::set<UnitCellCoord> nbors;
      flower_neighborhood(_tree.begin(), _tree.end(), std::inserter(nbors, nbors.begin()));


      ss << indent << "  m_neighborhood = std::set<Index> {\n" << indent;
      auto it = nbors.begin();
      while(it != nbors.end()) {
        ss <<  "  " <<  _nlist.neighbor_index(*it);
        //ss << indent << "    {UnitCellCoord("
        // << it->sublat() << ", "
        // << it->unitcell(0) << ", "
        // << it->unitcell(1) << ", "
        // << it->unitcell(2) << ")}";
        ++it;
        if(it != nbors.end()) {
          ss << ",";
        }
        ss << "\n";
      }
      ss << indent << "  };\n";
      ss << "\n\n";

      ss << indent <<  "  m_orbit_neighborhood.resize(corr_size());\n";
      Index lno = 0;
      for(Index no = 0; no < clex.n_orbits(); ++no) {
        std::set<UnitCellCoord> orbit_nbors;
        flower_neighborhood(_tree[no], std::inserter(orbit_nbors, orbit_nbors.begin()));

        Index proto_index = lno;

        ss << indent << "  m_orbit_neighborhood[" << lno << "] = std::set<Index> {\n" << indent;
        auto it = orbit_nbors.begin();
        while(it != orbit_nbors.end()) {
          ss <<  "  " <<  _nlist.neighbor_index(*it);
          //ss << indent << "    {UnitCellCoord("
          //<< it->sublat() << ", "
          // << it->unitcell(0) << ", "
          // << it->unitcell(1) << ", "
          // << it->unitcell(2) << ")}";
          ++it;
          if(it != orbit_nbors.end()) {
            ss << ",";
          }
          ss << "\n";
        }
        ss << indent << "  };\n";
        ++lno;
        for(Index nf = 1; nf < clex.bset_orbit(no).size(); ++nf) {
          ss << indent << "  m_orbit_neighborhood[" << lno << "] = m_orbit_neighborhood[" << proto_index << "];\n";
          ++lno;
        }
        ss << "\n";

      }


      ss <<
         indent << "}\n\n";

      return ss.str();
    }



    template<typename OrbitType>
    std::string clexulator_point_prepare_definition(std::string const &class_name,
                                                    ClexBasis const &clex,
                                                    std::vector<OrbitType> const &_tree,
                                                    std::vector<std::unique_ptr<OrbitFunctionTraits> > const &_orbit_func_traits,
                                                    std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                    PrimNeighborList &_nlist,
                                                    std::string const &indent) {

      std::string result("void " + class_name + "::_point_prepare(int neighbor_ind) const{\n");

      // Use known clexbasis dependencies to construct point_prepare routine
      for(auto const &doftype : clex.site_bases()) {
        result += DoFType::traits(doftype.first).clexulator_point_prepare_string(clex.prim(),
                                                                                 _nhood,
                                                                                 _nlist,
                                                                                 doftype.second,
                                                                                 indent);
      }

      for(auto const &func_trait : _orbit_func_traits) {
        result += func_trait->clexulator_point_prepare_string(clex.prim(),
                                                              _nhood,
                                                              _nlist,
                                                              indent);
      }
      result += "}\n";
      return result;
    }

    template<typename OrbitType>
    std::string clexulator_global_prepare_definition(std::string const &class_name,
                                                     ClexBasis const &clex,
                                                     std::vector<OrbitType> const &_tree,
                                                     std::vector<std::unique_ptr<OrbitFunctionTraits> > const &_orbit_func_traits,
                                                     std::map<UnitCellCoord, std::set<UnitCellCoord> > const &_nhood,
                                                     PrimNeighborList &_nlist,
                                                     std::string const &indent) {
      std::string result("void " + class_name + "::_global_prepare() const{\n");



      // Use known clexbasis dependencies to construct point_prepare routine
      for(auto const &doftype : clex.site_bases()) {
        result += DoFType::traits(doftype.first).clexulator_global_prepare_string(clex.prim(),
                                                                                  _nhood,
                                                                                  _nlist,
                                                                                  doftype.second,
                                                                                  indent);
      }

      // Use known clexbasis dependencies to construct point_prepare routine
      for(auto const &doftype : clex.global_bases()) {
        result += DoFType::traits(doftype.first).clexulator_global_prepare_string(clex.prim(),
                                                                                  _nhood,
                                                                                  _nlist,
                                                                                  std::vector<BasisSet>(1, doftype.second),
                                                                                  indent);
      }

      for(auto const &func_trait : _orbit_func_traits) {
        result += func_trait->clexulator_global_prepare_string(clex.prim(),
                                                               _nhood,
                                                               _nlist,
                                                               indent);
      }

      result += "}\n";
      return result;
    }


  }
}

#endif