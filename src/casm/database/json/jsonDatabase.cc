#include "casm/database/json/jsonDatabase.hh"
#include "casm/database/json/jsonPropertiesDatabase.hh"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include "casm/app/DirectoryStructure.hh"
#include "casm/clex/PrimClex_impl.hh"
#include "casm/casm_io/SafeOfstream.hh"
#include "casm/casm_io/json_io/container.hh"
#include "casm/database/DatabaseHandler_impl.hh"
#include "casm/database/Database_impl.hh"
#include "casm/database/DatabaseTypes_impl.hh"

//for testing:
#include "casm/casm_io/stream_io/container.hh"

namespace CASM {

  const std::string traits<DB::jsonDB>::name = "jsonDB";

  const std::string traits<DB::jsonDB>::version = "1.0";

  namespace DB {

    namespace {
      struct InsertImpl {
        InsertImpl(DatabaseHandler &_db_handler) : db_handler(_db_handler) {}
        DatabaseHandler &db_handler;

        template<typename T>
        void eval() {
          db_handler.insert<T>(
            traits<jsonDB>::name,
            notstd::make_unique<jsonDatabase<T> >(db_handler.primclex()));
        }
      };
    }

    namespace {

      struct InsertPropsImpl {
        InsertPropsImpl(DatabaseHandler &_db_handler) :
          db_handler(_db_handler),
          primclex(_db_handler.primclex()),
          dir(primclex.dir()),
          json_dir(dir.root_dir()) {}

        DatabaseHandler &db_handler;
        const PrimClex &primclex;
        const DirectoryStructure &dir;
        jsonDB::DirectoryStructure json_dir;

        template<typename T>
        void eval() {
          for(auto calc_type : dir.all_calctype()) {
            fs::path location = json_dir.props_list<T>(calc_type);
            db_handler.insert_props<T>(
              traits<jsonDB>::name,
              calc_type,
              notstd::make_unique<jsonPropertiesDatabase>(primclex, calc_type, location));
          }
        }
      };
    }


    void jsonDB::insert(DatabaseHandler &db_handler) {
      DB::for_each_type(InsertImpl(db_handler));
      DB::for_each_config_type(InsertPropsImpl(db_handler));
    }

    jsonDB::DirectoryStructure::DirectoryStructure(const fs::path _root) :
      m_dir(_root) {}

    template<typename DataObject>
    fs::path jsonDB::DirectoryStructure::obj_list() const {
      return m_dir.casm_dir() / traits<jsonDB>::name / (traits<DataObject>::short_name + "_list.json");
    }

    template<typename DataObject>
    fs::path jsonDB::DirectoryStructure::props_list(std::string calctype) const {
      return m_dir.casm_dir() / traits<jsonDB>::name / _calctype(calctype) / (traits<DataObject>::short_name + "_props.json");
    }


    std::string jsonDB::DirectoryStructure::_calctype(std::string calctype) const {
      return std::string("calctype.") + calctype;
    }


    jsonDatabase<Supercell>::jsonDatabase(const PrimClex &_primclex) :
      Database<Supercell>(_primclex),
      m_is_open(false) {}

    jsonDatabase<Supercell> &jsonDatabase<Supercell>::open() {

      if(m_is_open) {
        return *this;
      }

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      if(fs::exists(dir.obj_list<Supercell>())) {
        _read_scel_list();
      }
      else if(fs::exists(primclex().dir().SCEL())) {
        _read_SCEL();
      }

      this->read_aliases();

      m_is_open = true;
      return *this;
    }

    void jsonDatabase<Supercell>::commit() {
      jsonParser json;
      json["version"] = traits<jsonDB>::version;

      for(const auto &scel : *this) {
        json["supercells"][scel.name()] = scel.transf_mat();
      }

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());

      SafeOfstream file;
      fs::create_directories(dir.obj_list<Supercell>().parent_path());
      file.open(dir.obj_list<Supercell>());
      json.print(file.ofstream());
      file.close();

      this->write_aliases();
    }

    void jsonDatabase<Supercell>::close() {
      m_is_open = false;
      this->clear();
    }

    void jsonDatabase<Supercell>::_read_scel_list() {
      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      jsonParser json(dir.obj_list<Supercell>());

      // check json version
      if(!json.contains("version") || json["version"].get<std::string>() != traits<jsonDB>::version) {
        throw std::runtime_error(
          std::string("Error jsonDB version mismatch: found: ") +
          json["version"].get<std::string>() +
          " expected: " +
          traits<jsonDB>::version);
      }

      if(!json.is_obj() || !json.contains("supercells")) {
        throw std::runtime_error(
          std::string("Error invalid format: ") + dir.obj_list<Supercell>().string());
      }

      auto it = json["supercells"].begin();
      auto end = json["supercells"].end();
      for(; it != end; ++it) {
        Eigen::Matrix3i mat;
        from_json(mat, *it);
        this->emplace(&primclex(), mat);
      }
    }

    void jsonDatabase<Supercell>::_read_SCEL() {
      // expect a file with format:
      //
      // Supercell Number: 0 Volume: 1
      // Supercell Transformation Matrix:
      //  1 0 0
      //  0 1 0
      //  0 0 1
      //
      // Supercell Number: 1 Volume: 2
      // Supercell Transformation Matrix:
      //  1 0 -1
      //  0 1 0
      //  0 0 2

      Eigen::Matrix3i mat;

      std::string s;
      fs::ifstream stream(primclex().dir().SCEL());
      while(!stream.eof()) {
        std::getline(stream, s);
        if(s[0] == 'S') {
          std::getline(stream, s);
          stream >> mat;

          this->emplace(&primclex(), mat);
        }
      }
    }


    jsonDatabase<Configuration>::jsonDatabase(const PrimClex &_primclex) :
      Database<Configuration>(_primclex),
      m_is_open(false) {}

    jsonDatabase<Configuration> &jsonDatabase<Configuration>::open() {
      if(m_is_open) {
        return *this;
      }

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      fs::path config_list_path = dir.obj_list<Configuration>();

      if(!fs::exists(config_list_path)) {
        m_is_open = true;
        return *this;
      }

      jsonParser json(config_list_path);

      if(!json.is_obj() || !json.contains("supercells")) {
        throw std::runtime_error(
          std::string("Error invalid format: ") + config_list_path.string());
      }

      // check json version
      if(!json.contains("version") || json["version"].get<std::string>() != traits<jsonDB>::version) {
        throw std::runtime_error(
          std::string("Error jsonDB version mismatch: found: ") +
          json["version"].get<std::string>() +
          " expected: " +
          traits<jsonDB>::version);
      }

      // read config list contents
      auto scel_it = json["supercells"].begin();
      auto scel_end = json["supercells"].end();
      bool is_new = false;

      for(; scel_it != scel_end; ++scel_it) {

        auto config_it = scel_it->begin();
        auto config_end = scel_it->end();

        const Supercell &scel = *primclex().db_handler().
                                db<Supercell>(traits<jsonDB>::name).find(scel_it.name());

        for(; config_it != config_end; ++config_it) {
          auto result = m_config_list.emplace(scel, config_it.name(), *config_it);
          _on_insert_or_emplace(result, is_new);
        }
      }

      // read next config id for each supercell
      from_json(m_config_id, json["config_id"]);

      this->read_aliases();

      m_is_open = true;
      return *this;
    }

    void jsonDatabase<Configuration>::commit() {

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      fs::path config_list_path = dir.obj_list<Configuration>();
      if(primclex().db_handler().db<Supercell>(traits<jsonDB>::name).size() == 0) {
        fs::remove(config_list_path);
        return;
      }

      jsonParser json;

      if(fs::exists(config_list_path)) {
        json.read(config_list_path);
      }
      else {
        json.put_obj();
      }
      json["version"] = traits<jsonDB>::version;

      for(const auto &config : m_config_list) {
        config.to_json(json["supercells"][config.supercell().name()][config.id()]);
      }

      json["config_id"] = m_config_id;

      SafeOfstream file;
      fs::create_directories(config_list_path.parent_path());
      file.open(config_list_path);
      //json.print(file.ofstream());
      int indent = 0;
      int prec = 12;
      json_spirit::write_stream((json_spirit::mValue &) json, file.ofstream(), indent, prec),
                  file.close();

      this->write_aliases();
    }

    void jsonDatabase<Configuration>::close() {
      m_name_to_config.clear();
      m_config_list.clear();
      m_scel_range.clear();
      m_is_open = false;
    }

    jsonDatabase<Configuration>::iterator jsonDatabase<Configuration>::begin() const {
      return _iterator(m_config_list.begin());
    }

    jsonDatabase<Configuration>::iterator jsonDatabase<Configuration>::end() const {
      return _iterator(m_config_list.end());
    }

    jsonDatabase<Configuration>::size_type jsonDatabase<Configuration>::size() const {
      return m_config_list.size();
    }

    std::pair<jsonDatabase<Configuration>::iterator, bool> jsonDatabase<Configuration>::insert(const Configuration &config) {

      auto result = m_config_list.insert(config);

      return _on_insert_or_emplace(result, true);
    }

    std::pair<jsonDatabase<Configuration>::iterator, bool> jsonDatabase<Configuration>::insert(const Configuration &&config) {

      auto result = m_config_list.insert(std::move(config));

      return _on_insert_or_emplace(result, true);
    }

    jsonDatabase<Configuration>::iterator jsonDatabase<Configuration>::update(const Configuration &config) {

      ValDatabase<Configuration>::erase(config.name());
      return insert(config).first;
    }

    jsonDatabase<Configuration>::iterator jsonDatabase<Configuration>::erase(iterator pos) {

      // get m_config_list iterator
      auto base_it = static_cast<db_set_iterator *>(pos.get())->base();

      // erase name & alias
      m_name_to_config.erase(base_it->name());

      // update scel_range
      auto _scel_range_it = m_scel_range.find(base_it->supercell().name());
      if(_scel_range_it->second.first == _scel_range_it->second.second) {
        m_scel_range.erase(_scel_range_it);
      }
      else if(_scel_range_it->second.first == base_it) {
        ++(_scel_range_it->second.first);
      }
      else if(_scel_range_it->second.second == base_it) {
        --(_scel_range_it->second.second);
      }

      // erase Configuration
      return _iterator(m_config_list.erase(base_it));
    }

    jsonDatabase<Configuration>::iterator jsonDatabase<Configuration>::find(const std::string &name_or_alias) const {
      auto it = m_name_to_config.find(this->name(name_or_alias));
      if(it == m_name_to_config.end()) {
        return _iterator(m_config_list.end());
      }
      return _iterator(it->second);
    }

    /// Range of Configuration in a particular supecell
    boost::iterator_range<jsonDatabase<Configuration>::iterator>
    jsonDatabase<Configuration>::scel_range(const std::string &scelname) const {
      auto it = m_scel_range.find(scelname);
      if(it == m_scel_range.end()) {
        return boost::make_iterator_range(end(), end());
      }
      else {
        auto &res = it->second;
        return boost::make_iterator_range(_iterator(res.first), _iterator(std::next(res.second)));
      }
    }

    /// Find canonical Configuration in database by comparing DoF
    ///
    /// \param config A Configuration in canonical form
    ///
    /// - Find in set<Configuration>
    typename jsonDatabase<Configuration>::iterator
    jsonDatabase<Configuration>::search(const Configuration &config) const {

      // not clear if using m_scel_range to search on a sub-range would help...
      auto res = m_config_list.find(config);
      if(res == m_config_list.end()) {
        return end();
      }
      return _iterator(res);
    }

    /// Update m_name_to_config and m_scel_range after performing an insert or emplace
    std::pair<jsonDatabase<Configuration>::iterator, bool>
    jsonDatabase<Configuration>::_on_insert_or_emplace(std::pair<base_iterator, bool> &result, bool is_new) {

      if(result.second) {
        const Configuration &config = *result.first;
        assert(&config.primclex() == &primclex() && "jsonDatabase<Configuration>::_on_insert_or_emplace primclex does not match");

        if(is_new) {
          // set the config id, and increment
          auto _config_id_it = m_config_id.find(config.supercell().name());
          if(_config_id_it == m_config_id.end()) {
            _config_id_it = m_config_id.insert(
                              std::make_pair(config.supercell().name(), 0)).first;
          }
          this->set_id(config, _config_id_it->second++);
        }

        // update name -> config
        m_name_to_config.insert(std::make_pair(config.name(), result.first));

        // check if scel_range needs updating
        auto _scel_range_it = m_scel_range.find(config.supercell().name());

        // new supercell
        if(_scel_range_it == m_scel_range.end()) {
          m_scel_range.emplace(
            config.supercell().name(),
            std::make_pair(result.first, result.first));
        }
        // if new 'begin' of scel range
        else if(_scel_range_it->second.first == std::next(result.first)) {
          _scel_range_it->second.first = result.first;
        }
        // if new 'end' of scel range (!= past-the-last config in scel)
        else if(_scel_range_it->second.second == std::prev(result.first)) {
          _scel_range_it->second.second = result.first;
        }
      }

      return std::make_pair(_iterator(result.first), result.second);
    }


    /// PPDTO stuff starts here
    jsonDatabase<PrimPeriodicDiffTransOrbit>::jsonDatabase(const PrimClex &_primclex) :
      Database<PrimPeriodicDiffTransOrbit>(_primclex),
      m_is_open(false), m_orbit_id(0) {}


    jsonDatabase<PrimPeriodicDiffTransOrbit> &jsonDatabase<PrimPeriodicDiffTransOrbit>::open() {

      if(m_is_open) {
        return *this;
      }

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      fs::path diff_trans_list_path = dir.obj_list<PrimPeriodicDiffTransOrbit>();
      if(!fs::exists(diff_trans_list_path)) {
        m_is_open = true;
        return *this;
      }
      jsonParser json(diff_trans_list_path);
      // check json version
      if(!json.contains("version") || json["version"].get<std::string>() != traits<jsonDB>::version) {
        throw std::runtime_error(
          std::string("Error jsonDB version mismatch: found: ") +
          json["version"].get<std::string>() +
          " expected: " +
          traits<jsonDB>::version);
      }
      if(!json.is_obj() || !json.contains("prototypes")) {
        throw std::runtime_error(
          std::string("Error invalid format: ") + dir.obj_list<PrimPeriodicDiffTransOrbit>().string());
      }
      auto end = json["prototypes"].end();
      for(auto it = json["prototypes"].begin(); it != end; ++it) {
        Kinetics::DiffusionTransformation trans = jsonConstructor<Kinetics::DiffusionTransformation>::from_json(*it, primclex().prim());
        PrimPeriodicSymCompare<Kinetics::DiffusionTransformation> symcompare(primclex().crystallography_tol());
        auto result = m_orbit_list.emplace(trans, primclex().prim().factor_group(), symcompare, &primclex());
        this->set_id(*(result.first), it.name());
        _on_insert_or_emplace(result, false);
      }

      //read next orbit id
      from_json(m_orbit_id, json["orbit_id"]);

      this->read_aliases();

      m_is_open = true;
      return *this;

    }

    void jsonDatabase<PrimPeriodicDiffTransOrbit>::commit() {

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      fs::path orbit_list_path = dir.obj_list<PrimPeriodicDiffTransOrbit>();

      jsonParser json;

      if(fs::exists(orbit_list_path)) {
        json.read(orbit_list_path);
      }
      else {
        json.put_obj();
      }
      json["version"] = traits<jsonDB>::version;
      json["prototypes"].put_obj();
      for(const auto &orbit : m_orbit_list) {
        to_json(orbit.prototype(), json["prototypes"][orbit.id()]);
      }

      json["orbit_id"] = m_orbit_id;

      SafeOfstream file;
      fs::create_directories(orbit_list_path.parent_path());
      file.open(orbit_list_path);
      json.print(file.ofstream());
      /*int indent = 0;
      int prec = 12;
      json_spirit::write_stream((json_spirit::mValue &) json, file.ofstream(), indent, prec),
                  file.close();*/
      file.close();
      this->write_aliases();
    }

    void jsonDatabase<PrimPeriodicDiffTransOrbit>::close() {
      m_name_to_orbit.clear();
      m_orbit_list.clear();
      m_is_open = false;
    }

    jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator
    jsonDatabase<PrimPeriodicDiffTransOrbit>::begin() const {
      return _iterator(m_orbit_list.begin());
    }

    jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator
    jsonDatabase<PrimPeriodicDiffTransOrbit>::end() const {
      return _iterator(m_orbit_list.end());
    }

    jsonDatabase<PrimPeriodicDiffTransOrbit>::size_type
    jsonDatabase<PrimPeriodicDiffTransOrbit>::size() const {
      return m_orbit_list.size();
    }

    std::pair<jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator, bool>
    jsonDatabase<PrimPeriodicDiffTransOrbit>::insert(const PrimPeriodicDiffTransOrbit &orbit) {

      auto result = m_orbit_list.insert(orbit);

      return _on_insert_or_emplace(result, true);
    }

    std::pair<jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator, bool>
    jsonDatabase<PrimPeriodicDiffTransOrbit>::insert(const PrimPeriodicDiffTransOrbit &&orbit) {

      auto result = m_orbit_list.insert(std::move(orbit));

      return _on_insert_or_emplace(result, true);
    }

    jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator
    jsonDatabase<PrimPeriodicDiffTransOrbit>::erase(iterator pos) {

      // get m_orbit_list iterator
      auto base_it = static_cast<db_set_iterator *>(pos.get())->base();

      // erase name & alias
      m_name_to_orbit.erase(base_it->name());

      // erase PrimPeriodicDiffTransOrbit
      return _iterator(m_orbit_list.erase(base_it));
    }

    jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator
    jsonDatabase<PrimPeriodicDiffTransOrbit>::find(const std::string &name_or_alias) const {
      auto it = m_name_to_orbit.find(this->name(name_or_alias));
      if(it == m_name_to_orbit.end()) {
        return _iterator(m_orbit_list.end());
      }
      return _iterator(it->second);
    }

    /// Find PrimPeriodicDiffTransOrbit in database by comparing prototype
    typename jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator
    jsonDatabase<PrimPeriodicDiffTransOrbit>::search(const PrimPeriodicDiffTransOrbit &orbit) const {
      return _iterator(m_orbit_list.find(orbit));
    }

    /// Find DiffusionTransformation in database by comparing to orbit prototypes
    typename jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator
    jsonDatabase<PrimPeriodicDiffTransOrbit>::search(const Kinetics::DiffusionTransformation &diff_trans) const {

      const auto &g = prim().factor_group();
      PrimPeriodicDiffTransSymCompare sym_compare(crystallography_tol());
      CanonicalGenerator<PrimPeriodicDiffTransOrbit> gen(g, sym_compare);
      auto canon_diff_trans = gen(diff_trans);
      auto f = [&](const PrimPeriodicDiffTransOrbit & orbit) {
        return sym_compare.equal(canon_diff_trans, gen(orbit.prototype()));
      };

      return _iterator(std::find_if(m_orbit_list.begin(), m_orbit_list.end(), f));
    }

    /// Update m_name_to_orbit after performing an insert or emplace
    std::pair<jsonDatabase<PrimPeriodicDiffTransOrbit>::iterator, bool>
    jsonDatabase<PrimPeriodicDiffTransOrbit>::_on_insert_or_emplace(std::pair<base_iterator, bool> &result, bool is_new) {

      if(result.second) {
        const PrimPeriodicDiffTransOrbit &orbit = *result.first;
        if(is_new) {
          // set the orbit id, and increment
          this->set_id(orbit, m_orbit_id++);
        }

        // update name -> orbit
        m_name_to_orbit.insert(std::make_pair(orbit.name(), result.first));

      }
      return std::make_pair(_iterator(result.first), result.second);
    }

    // BEGIN DiffTransConfiguration stuff
    jsonDatabase<Kinetics::DiffTransConfiguration>::jsonDatabase(const PrimClex &_primclex) :
      Database<Kinetics::DiffTransConfiguration>(_primclex),
      m_is_open(false) {}

    jsonDatabase<Kinetics::DiffTransConfiguration> &jsonDatabase<Kinetics::DiffTransConfiguration>::open() {
      if(m_is_open) {
        return *this;
      }

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      fs::path diff_trans_config_list_path = dir.obj_list<Kinetics::DiffTransConfiguration>();

      if(!fs::exists(diff_trans_config_list_path)) {
        m_is_open = true;
        return *this;
      }

      jsonParser json(diff_trans_config_list_path);

      if(!json.is_obj() || !json.contains("prototypes")) {
        throw std::runtime_error(
          std::string("Error invalid format: ") + diff_trans_config_list_path.string());
      }

      // check json version
      if(!json.contains("version") || json["version"].get<std::string>() != traits<jsonDB>::version) {
        throw std::runtime_error(
          std::string("Error jsonDB version mismatch: found: ") +
          json["version"].get<std::string>() +
          " expected: " +
          traits<jsonDB>::version);
      }

      // read diff_trans_config list contents
      auto orbit_it = json["prototypes"].begin();
      auto orbit_end = json["prototypes"].end();

      for(; orbit_it != orbit_end; ++orbit_it) {

        auto scel_it = orbit_it->begin();
        auto scel_end = orbit_it->end();
        bool is_new = false;

        for(; scel_it != scel_end; ++scel_it) {

          auto diff_trans_config_it = scel_it->begin();
          auto diff_trans_config_end = scel_it->end();

          const Supercell &scel = *primclex().db_handler().
                                  db<Supercell>(traits<jsonDB>::name).find(scel_it.name());

          for(; diff_trans_config_it != diff_trans_config_end; ++diff_trans_config_it) {
            auto result = m_diff_trans_config_list.emplace(scel, *diff_trans_config_it);
            this->set_id(*(result.first), diff_trans_config_it.name());
            _on_insert_or_emplace(result, is_new);
          }
        }
      }

      // read next config id for each supercell
      from_json(m_config_id, json["config_id"]);

      this->read_aliases();

      m_is_open = true;
      return *this;
    }

    void jsonDatabase<Kinetics::DiffTransConfiguration>::commit() {

      jsonDB::DirectoryStructure dir(primclex().dir().root_dir());
      fs::path diff_trans_config_list_path = dir.obj_list<Kinetics::DiffTransConfiguration>();
      if(primclex().db_handler().db<Supercell>(traits<jsonDB>::name).size() == 0) {
        fs::remove(diff_trans_config_list_path);
        return;
      }

      jsonParser json;

      if(fs::exists(diff_trans_config_list_path)) {
        json.read(diff_trans_config_list_path);
      }
      else {
        json.put_obj();
      }
      json["version"] = traits<jsonDB>::version;

      //This is going to be problematic Need to figure out how to preserve orbit name source
      for(const auto &diff_trans_config : m_diff_trans_config_list) {
        std::string dtconfig_name = diff_trans_config.orbit_name();
        std::string scelname = diff_trans_config.from_config().supercell().name();
        diff_trans_config.to_json(
          json["prototypes"][dtconfig_name][scelname][diff_trans_config.id()]);
      }

      json["config_id"] = m_config_id;

      SafeOfstream file;
      fs::create_directories(diff_trans_config_list_path.parent_path());
      file.open(diff_trans_config_list_path);
      //json.print(file.ofstream());
      int indent = 0;
      int prec = 12;
      json_spirit::write_stream((json_spirit::mValue &) json, file.ofstream(), indent, prec),
                  file.close();

      this->write_aliases();
    }

    void jsonDatabase<Kinetics::DiffTransConfiguration>::close() {
      m_name_to_diff_trans_config.clear();
      m_diff_trans_config_list.clear();
      m_scel_range.clear();
      m_orbit_range.clear();
      m_orbit_scel_range.clear();
      m_is_open = false;
    }

    jsonDatabase<Kinetics::DiffTransConfiguration>::iterator jsonDatabase<Kinetics::DiffTransConfiguration>::begin() const {
      return _iterator(m_diff_trans_config_list.begin());
    }

    jsonDatabase<Kinetics::DiffTransConfiguration>::iterator jsonDatabase<Kinetics::DiffTransConfiguration>::end() const {
      return _iterator(m_diff_trans_config_list.end());
    }

    jsonDatabase<Kinetics::DiffTransConfiguration>::size_type jsonDatabase<Kinetics::DiffTransConfiguration>::size() const {
      return m_diff_trans_config_list.size();
    }

    std::pair<jsonDatabase<Kinetics::DiffTransConfiguration>::iterator, bool> jsonDatabase<Kinetics::DiffTransConfiguration>::insert(const Kinetics::DiffTransConfiguration &diff_trans_config) {

      auto result = m_diff_trans_config_list.insert(diff_trans_config);

      return _on_insert_or_emplace(result, true);
    }

    std::pair<jsonDatabase<Kinetics::DiffTransConfiguration>::iterator, bool> jsonDatabase<Kinetics::DiffTransConfiguration>::insert(const Kinetics::DiffTransConfiguration &&diff_trans_config) {

      auto result = m_diff_trans_config_list.insert(std::move(diff_trans_config));

      return _on_insert_or_emplace(result, true);
    }

    jsonDatabase<Kinetics::DiffTransConfiguration>::iterator jsonDatabase<Kinetics::DiffTransConfiguration>::update(const Kinetics::DiffTransConfiguration &diff_trans_config) {

      ValDatabase<Kinetics::DiffTransConfiguration>::erase(diff_trans_config.name());
      return insert(diff_trans_config).first;
    }

    //NEEDS TO be changed for dtc
    jsonDatabase<Kinetics::DiffTransConfiguration>::iterator jsonDatabase<Kinetics::DiffTransConfiguration>::erase(iterator pos) {

      // get m_diff_trans_config_list iterator
      auto base_it = static_cast<db_set_iterator *>(pos.get())->base();

      // erase name & alias
      m_name_to_diff_trans_config.erase(base_it->name());

      // update scel_range
      auto _scel_range_it = m_scel_range.find(base_it->from_config().supercell().name());
      if(_scel_range_it->second.first == _scel_range_it->second.second) {
        m_scel_range.erase(_scel_range_it);
      }
      else if(_scel_range_it->second.first == base_it) {
        ++(_scel_range_it->second.first);
      }
      else if(_scel_range_it->second.second == base_it) {
        --(_scel_range_it->second.second);
      }

      // update orbit_range
      std::string dt_name = base_it->orbit_name();
      auto _orbit_range_it = m_orbit_range.find(dt_name);
      if(_orbit_range_it->second.first == _orbit_range_it->second.second) {
        m_orbit_range.erase(_orbit_range_it);
      }
      else if(_orbit_range_it->second.first == base_it) {
        ++(_orbit_range_it->second.first);
      }
      else if(_orbit_range_it->second.second == base_it) {
        --(_orbit_range_it->second.second);
      }

      // update orbit_scel_range
      auto _orbit_scel_range_it = m_orbit_scel_range.find(dt_name);
      auto _sub_it = (_orbit_scel_range_it->second).find(base_it->from_config().supercell().name());
      if(_sub_it->second.first == _sub_it->second.second) {
        (_orbit_scel_range_it->second).erase(_sub_it);
        if(!(_orbit_scel_range_it->second).size()) {
          m_orbit_scel_range.erase(_orbit_scel_range_it);
        }
      }
      else if(_sub_it->second.first == base_it) {
        ++(_sub_it->second.first);
      }
      else if(_sub_it->second.second == base_it) {
        --(_sub_it->second.second);
      }

      // erase DiffTransConfiguration
      return _iterator(m_diff_trans_config_list.erase(base_it));
    }

    jsonDatabase<Kinetics::DiffTransConfiguration>::iterator jsonDatabase<Kinetics::DiffTransConfiguration>::find(const std::string &name_or_alias) const {
      auto it = m_name_to_diff_trans_config.find(this->name(name_or_alias));
      if(it == m_name_to_diff_trans_config.end()) {
        return _iterator(m_diff_trans_config_list.end());
      }
      return _iterator(it->second);
    }

    /// Range of DiffTransConfiguration in a particular supercell
    boost::iterator_range<jsonDatabase<Kinetics::DiffTransConfiguration>::iterator>
    jsonDatabase<Kinetics::DiffTransConfiguration>::scel_range(const std::string &scelname) const {
      auto it = m_scel_range.find(scelname);
      if(it == m_scel_range.end()) {
        return boost::make_iterator_range(end(), end());
      }
      else {
        auto &res = it->second;
        return boost::make_iterator_range(_iterator(res.first), _iterator(std::next(res.second)));
      }
    }

    /// Range of DiffTransConfiguration in a particular orbit
    boost::iterator_range<jsonDatabase<Kinetics::DiffTransConfiguration>::iterator>
    jsonDatabase<Kinetics::DiffTransConfiguration>::orbit_range(const std::string &diff_trans_name) const {
      auto it = m_orbit_range.find(diff_trans_name);
      if(it == m_orbit_range.end()) {
        return boost::make_iterator_range(end(), end());
      }
      else {
        auto &res = it->second;
        return boost::make_iterator_range(_iterator(res.first), _iterator(std::next(res.second)));
      }
    }

    /// Range of DiffTransConfiguration in a particular supercell within an orbit
    boost::iterator_range<jsonDatabase<Kinetics::DiffTransConfiguration>::iterator>
    jsonDatabase<Kinetics::DiffTransConfiguration>::orbit_scel_range(const std::string &diff_trans_name, const std::string &scelname) const {
      auto it = m_orbit_scel_range.find(diff_trans_name);
      if(it == m_orbit_scel_range.end()) {
        return boost::make_iterator_range(end(), end());
      }
      else {
        auto it2 = (it->second).find(diff_trans_name);
        if(it2 == (it->second).end()) {
          return boost::make_iterator_range(end(), end());
        }
        else {
          auto &res = it2->second;
          return boost::make_iterator_range(_iterator(res.first), _iterator(std::next(res.second)));
        }
      }
    }

    /// Update m_name_to_diff_trans_config, m_orbit_scel_range, m_orbit_range, m_scel_range after performing an insert or emplace
    std::pair<jsonDatabase<Kinetics::DiffTransConfiguration>::iterator, bool>
    jsonDatabase<Kinetics::DiffTransConfiguration>::_on_insert_or_emplace(std::pair<base_iterator, bool> &result, bool is_new) {

      if(result.second) {

        const Kinetics::DiffTransConfiguration &diff_trans_config = *result.first;
        std::string dt_name;
        if(is_new) {
          // set the diff trans config id, and increment
          //Again need to determine orbit name from diff_trans_config object somehow
          dt_name = diff_trans_config.orbit_name();
          std::string scelname = diff_trans_config.from_config().supercell().name();
          auto _config_id_it = m_config_id.find(dt_name);
          if(_config_id_it == m_config_id.end()) {
            std::map<std::string, Index> tmp ;
            tmp.insert(std::make_pair(scelname, 0));
            _config_id_it = m_config_id.insert(
                              std::make_pair(dt_name, tmp)).first;
          }
          else {
            auto it2 = (_config_id_it->second).find(scelname);
            if(it2 == (_config_id_it->second).end()) {
              (_config_id_it->second).insert(std::make_pair(scelname, 0));
            }
          }
          this->set_id(diff_trans_config, ((_config_id_it->second).find(scelname))->second++);

        }

        // update name -> config
        m_name_to_diff_trans_config.insert(std::make_pair(diff_trans_config.name(), result.first));

        // check if scel_range needs updating
        auto _scel_range_it = m_scel_range.find(diff_trans_config.from_config().supercell().name());

        // new supercell
        if(_scel_range_it == m_scel_range.end()) {
          m_scel_range.emplace(
            diff_trans_config.from_config().supercell().name(),
            std::make_pair(result.first, result.first));
        }
        // if new 'begin' of scel range
        else if(_scel_range_it->second.first == std::next(result.first)) {
          _scel_range_it->second.first = result.first;
        }
        // if new 'end' of scel range (!= past-the-last config in scel)
        else if(_scel_range_it->second.second == std::prev(result.first)) {
          _scel_range_it->second.second = result.first;
        }

        // check if orbit_range needs updating
        dt_name = diff_trans_config.orbit_name();
        auto _orbit_range_it = m_orbit_range.find(dt_name);

        // new orbit
        if(_orbit_range_it == m_orbit_range.end()) {
          m_orbit_range.emplace(
            dt_name,
            std::make_pair(result.first, result.first));
        }
        // if new 'begin' of orbit range
        else if(_orbit_range_it->second.first == std::next(result.first)) {
          _orbit_range_it->second.first = result.first;
        }
        // if new 'end' of orbit range (!= past-the-last config in scel)
        else if(_orbit_range_it->second.second == std::prev(result.first)) {
          _orbit_range_it->second.second = result.first;
        }

        // check if orbit_scel_range needs updating
        auto _orbit_scel_range_it = m_orbit_scel_range.find(dt_name);

        // new supercell
        if(_orbit_scel_range_it == m_orbit_scel_range.end()) {
          std::map<std::string, std::pair<base_iterator, base_iterator>> tmp;
          tmp.emplace(diff_trans_config.from_config().supercell().name(),
                      std::make_pair(result.first, result.first));
          m_orbit_scel_range.emplace(dt_name, tmp);
        }
        else if((_orbit_scel_range_it->second).find(diff_trans_config.from_config().supercell().name())
                == (_orbit_scel_range_it->second).end()) {
          (_orbit_scel_range_it->second).emplace(diff_trans_config.from_config().supercell().name(),
                                                 std::make_pair(result.first, result.first));
        }
        // if new 'begin' of scel range
        else if(((_orbit_scel_range_it->second).find(diff_trans_config.from_config().supercell().name()))->second.first == std::next(result.first)) {
          ((_orbit_scel_range_it->second).find(diff_trans_config.from_config().supercell().name()))->second.first = result.first;
        }
        // if new 'end' of scel range (!= past-the-last config in scel)
        else if(((_orbit_scel_range_it->second).find(diff_trans_config.from_config().supercell().name()))->second.second == std::prev(result.first)) {
          ((_orbit_scel_range_it->second).find(diff_trans_config.from_config().supercell().name()))->second.second = result.first;
        }

      }

      return std::make_pair(_iterator(result.first), result.second);
    }

  }
}

// explicit template instantiations
#define INST_jsonDB(r, data, type) \
template fs::path jsonDB::DirectoryStructure::obj_list<type>() const; \

#define INST_jsonDB_config(r, data, type) \
template fs::path jsonDB::DirectoryStructure::props_list<type>(std::string calctype) const; \

namespace CASM {
  namespace DB {

    BOOST_PP_SEQ_FOR_EACH(INST_jsonDB, _, CASM_DB_TYPES)
    BOOST_PP_SEQ_FOR_EACH(INST_jsonDB_config, _, CASM_DB_CONFIG_TYPES)
  }
}
