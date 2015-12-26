#include "casm/casm_io/json_io/clex.hh"
#include "casm/crystallography/Structure.hh"
#include "casm/clex/ChemicalReference.hh"

#include "casm/casm_io/json_io/container.hh"

namespace CASM {
  
  // --- ChemicalReferenceState -------------------
  
  /// \brief Write ChemicalReferenceState to: '{"A" : X, "B" : X, ..., "energy_per_species" : X }'
  jsonParser &to_json(const ChemicalReferenceState &ref_state, jsonParser &json) {
    to_json(ref_state.species_num, json);
    to_json(ref_state.energy_per_species, json["energy_per_species"]);
    return json;
  }
  
  /// \brief Read ChemicalReferenceState from: '{"A" : X, "B" : X, ..., "energy_per_species" : X }'
  template<> 
  ChemicalReferenceState from_json(const jsonParser &json) {
    ChemicalReferenceState ref_state;
    bool found_value = false;
    for(auto it=json.begin(); it!=json.end(); ++it) {
      if(it.name() == "energy_per_species") {
        found_value = true;
        from_json(ref_state.energy_per_species, *it);
      }
      else if(is_vacancy(it.name())) {
        std::cerr << "Error reading chemical reference state: " << json << std::endl;
        throw std::runtime_error("Error reading chemical reference: Input should not include vacancies");
      }
      else {
        from_json(ref_state.species_num[it.name()], *it);
      }
    }
    if(!found_value) {
      std::cerr << "Error reading chemical reference state: " << json << std::endl;
      throw std::runtime_error("Error reading chemical reference: No 'energy_per_species' found");
    }
    return ref_state;
  }

  /// \brief Read ChemicalReferenceState from: '{"A" : X, "B" : X, ..., "energy_per_species" : X }'
  void from_json(ChemicalReferenceState &ref_state, const jsonParser &json) {
    ref_state = from_json<ChemicalReferenceState>(json);
  }
  
  
  // --- HyperPlaneReference -------------------
  
  jsonParser &to_json(const HyperPlaneReference &ref, jsonParser &json) {
    json.put_obj();
    to_json(ref.global(), json["global"]);
    to_json(ref.supercell(), json["supercell"]);
    to_json(ref.supercell(), json["config"]);
    return json;
  }
  
  HyperPlaneReference jsonConstructor<HyperPlaneReference>::from_json(
    const jsonParser &json, 
    HyperPlaneReference::InputFunction f) {
    
    HyperPlaneReference ref(CASM::from_json<Eigen::VectorXd>(json["global"]), f);
    CASM::from_json(ref.supercell(), json["supercell"]);
    CASM::from_json(ref.config(), json["config"]);
    return ref;
  }

  void from_json(HyperPlaneReference &ref, 
                 const jsonParser &json, 
                 HyperPlaneReference::InputFunction f) {
    ref = jsonConstructor<HyperPlaneReference>::from_json(json, f);
  }
  
  
  // --- ChemicalReference -------------------
  
  /// \brief Write ChemicalReference
  ///
  /// Example form:
  /// \code
  /// { 
  ///   "chemical_reference" : {
  ///     "global" : ...,
  ///     "supercell": {
  ///       "SCELX": ...,
  ///       "SCELY": ...
  ///     },
  ///     "config": {
  ///       "SCELX/I": ...,
  ///       "SCELY/J": ...
  ///     }
  ///   }
  /// }
  /// \endcode
  ///
  /// Each individual reference is a vector, 
  /// \code
  /// [X, X, X, X]
  /// \endcode
  /// giving the hyperplane of the reference (each element is the reference
  /// value for pure Configurations of a given Molecule).
  ///
  jsonParser &to_json(const ChemicalReference &ref, jsonParser &json) {
    json.put_obj();
    
    json["species_order"] = ref.prim().get_struc_molecule_name();
    
    if(ref.global_ref_states().empty()) {
      to_json(ref.global(), json["global"]);
    }
    else {
      to_json(ref.global_ref_states(), json["global"]);
    }
    
    jsonParser& s_json = json["supercell"];
    for(auto it=ref.supercell().begin(); it!=ref.supercell().end(); ++it) {
      auto res = ref.supercell_ref_states().find(it->first);
      if(res == ref.supercell_ref_states().end()) {
        to_json(it->second, s_json[it->first]);
      }
      else {
        to_json(res->second, s_json[it->first]);
      }
    }
    
    jsonParser& c_json = json["config"];
    for(auto it=ref.config().begin(); it!=ref.supercell().end(); ++it) {
      auto res = ref.config_ref_states().find(it->first);
      if(res == ref.config_ref_states().end()) {
        to_json(it->second, c_json[it->first]);
      }
      else {
        to_json(res->second, c_json[it->first]);
      }
    }
    return json;
  }
  
  /// \brief Read chemical reference from one of 3 alternative forms
  ///
  /// This function returns a pair with only one element initialized. 
  /// If alternative 1, then the vector is set. If alternative 2 or 3, then the 
  /// Eigen::VectorXd is set.
  ///
  /// Expected input form:
  /// 1)
  ///   \code
  ///   [
  ///     {"A": 3.4, "C": 2.0, "energy_per_species": 2.0}, 
  ///     {"B": 2.0, "energy_per_species": 4.0}, 
  ///     {"C": 1.0, "energy_per_species": 3.0}
  ///   ]
  ///   \endcode
  /// 2) Expects all species in prim, except vacancy:
  ///   \code
  ///   {"A": X, "C": X, "D": X} 
  ///   \endcode
  /// 3) Expects one element for each species in prim, including 0.0 for vacancy:
  ///  \code
  ///   [X, X, X, X] 
  ///   \endcode
  ///
  std::pair<Eigen::VectorXd, std::vector<ChemicalReferenceState> >
  one_chemical_reference_from_json(const Structure& prim, 
                                   const jsonParser& json) {
    
    typedef std::pair<Eigen::VectorXd, std::vector<ChemicalReferenceState> > ReturnType;
    
    ReturnType result;
    
    std::vector<std::string> struc_mol_name = prim.get_struc_molecule_name();
    
    // if: {"A": X, "C": X, "D": X} // expects all species in prim, except vacancy
    if(json.is_obj()) {
      Eigen::VectorXd P = Eigen::VectorXd::Zero(struc_mol_name.size());
      for(int i=0; i<struc_mol_name.size(); ++i) {
        if(!is_vacancy(struc_mol_name[i])) {
          if(json.find(struc_mol_name[i]) != json.end()) {
            from_json(P(i), json[struc_mol_name[i]]);
          }
          else {
            std::cerr << "Error reading chemical reference: " << json << std::endl;
            throw std::runtime_error("Error reading chemical reference: Could not find " + struc_mol_name[i]);
          }
        }
      }
      result.first = P;
      return result;
    }
    else {
      
      // if: [X, X, X, X] 
      if(json.begin()->is_number()) {
        result.first = json.get<Eigen::VectorXd>();
        return result;
      }
      
      // [
      //   {"A": 3.4, "C": 2.0, "energy_per_species": 2.0}, 
      //   {"B": 2.0, "energy_per_species": 4.0}, 
      //   {"C": 1.0, "energy_per_species": 3.0}
      // ]
      else {
        std::for_each(json.begin(),
                      json.end(),
                      [&](const jsonParser& json){
                        result.second.push_back(json.get<ChemicalReferenceState>());
                      });
        return result;
      }
    }
  }
  
  /// \brief Read chemical reference from JSON
  ///
  /// Example expected JSON form:
  /// \code
  /// {
  ///   "global" : ...,
  ///   "supercell": {
  ///     "SCELX": ...,
  ///     "SCELY": ...
  ///   },
  ///   "config": {
  ///     "SCELX/I": ...,
  ///     "SCELY/J": ...
  ///   }
  /// }
  /// \endcode
  ///
  /// See one_chemical_reference_from_json for documentation of the \code {...} 
  /// \endcode expected form.
  ChemicalReference jsonConstructor<ChemicalReference>::from_json(
    const jsonParser &json,
    const Structure& prim, 
    double tol) {
    
    std::unique_ptr<ChemicalReference> ref;
    
    auto res = one_chemical_reference_from_json(prim, json["global"]);
    if(res.second.empty()) {
      ref = notstd::make_unique<ChemicalReference>(prim, res.first);
    }
    else {
      ref = notstd::make_unique<ChemicalReference>(prim, res.second.begin(), res.second.end(), tol);
    }
    
    if(json.find("supercell") != json.end()) {
      for(auto it=json["supercell"].begin(); it!=json["supercell"].end(); ++it) {
        auto res = one_chemical_reference_from_json(prim, *it);
        if(res.second.empty()) {
          ref->set_supercell(it.name(), res.first);
        }
        else {
          ref->set_supercell(it.name(), res.second.begin(), res.second.end(), tol);
        }
      }
    }
    
    if(json.find("config") != json.end()) {
      for(auto it=json["config"].begin(); it!=json["config"].end(); ++it) {
        auto res = one_chemical_reference_from_json(prim, *it);
        if(res.second.empty()) {
          ref->set_config(it.name(), res.first);
        }
        else {
          ref->set_config(it.name(), res.second.begin(), res.second.end(), tol);
        }
      }
    }
    
    return std::move(*ref);
  }
  

  /// \brief Read chemical reference from JSON
  void from_json(ChemicalReference &ref, 
                 const jsonParser &json,
                 const Structure& prim, 
                 double tol) {
    ref = jsonConstructor<ChemicalReference>::from_json(json, prim, tol);
  }
  
}
