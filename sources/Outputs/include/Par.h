//
// Copyright (c) 2020, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//

/**
 * @file  Par.h
 *
 * @brief Dynaflow launcher PAR file writer header file
 *
 */

#pragma once

#include "Configuration.h"
#include "DynModelDefinitionAlgorithm.h"
#include "DynamicDataBaseManager.h"
#include "GeneratorDefinitionAlgorithm.h"
#include "HVDCDefinitionAlgorithm.h"
#include "LineDefinitionAlgorithm.h"
#include "LoadDefinitionAlgorithm.h"
#include "ParDynModel.h"
#include "ParGenerator.h"
#include "ParHvdc.h"
#include "ParLoads.h"
#include "ParSVarC.h"
#include "SVarCDefinitionAlgorithm.h"
#include "ShuntDefinitionAlgorithm.h"

#include <string>
#include <vector>

namespace dfl {
namespace outputs {
/**
 * @brief PAR file writer
 */
class Par {
 public:
  /**
   * @brief PAR file definition
   */
  struct ParDefinition {
    /**
     * @brief Constructor
     *
     * @param base basename for current simulation
     * @param dir the dirname of the output PAR file
     * @param filename file path for output PAR file (corresponds to basename)
     * @param gens list of the generators taken into account
     * @param hvdcDefinitions HVDC lines definitions
     * @param activePowerCompensation the type of active power compensation
     * @param busesWithDynamicModel map of bus ids to a generator that regulates them
     * @param dynamicDataBaseManager dynamic database manager to use
     * @param counters the counters definitions to use
     * @param models list of dynamic models definitions
     * @param linesById the lines to use
     * @param svarcsDefinitions the SVarC definitions to use
     * @param loadsDefinitions the loads definitions to use
     */
    ParDefinition(const std::string& base, const boost::filesystem::path& dir, const boost::filesystem::path& filename,
                  const std::vector<algo::GeneratorDefinition>& gens, const algo::HVDCLineDefinitions& hvdcDefinitions,
                  inputs::Configuration::ActivePowerCompensation activePowerCompensation,
                  const algo::GeneratorDefinitionAlgorithm::BusGenMap& busesWithDynamicModel, const inputs::DynamicDataBaseManager& dynamicDataBaseManager,
                  const algo::ShuntCounterDefinitions& counters, const algo::DynamicModelDefinitions& models, const algo::LinesByIdDefinitions& linesById,
                  const std::vector<algo::StaticVarCompensatorDefinition>& svarcsDefinitions, const std::vector<algo::LoadDefinition>& loadsDefinitions) :
        basename_(base),
        dirname_(dir),
        filepath_(filename),
        activePowerCompensation_(activePowerCompensation),
        busesWithDynamicModel_(busesWithDynamicModel),
        dynamicDataBaseManager_(dynamicDataBaseManager),
        shuntCounters_(counters),
        linesByIdDefinitions_(linesById),
        parLoads_(new ParLoads(loadsDefinitions)),
        parSVarC_(new ParSVarC(svarcsDefinitions)),
        parHvdc_(new ParHvdc(hvdcDefinitions)),
        parGenerator_(new ParGenerator(gens)),
        parDynModel_(new ParDynModel(models)) {}

    std::string basename_;                                                         ///< basename
    boost::filesystem::path dirname_;                                              ///< Dirname of output file relative to execution dir
    boost::filesystem::path filepath_;                                             ///< file path of the output file to write
    dfl::inputs::Configuration::ActivePowerCompensation activePowerCompensation_;  ///< the type of active power compensation
    const algo::GeneratorDefinitionAlgorithm::BusGenMap& busesWithDynamicModel_;   ///< map of bus ids to a generator that regulates them
    const inputs::DynamicDataBaseManager& dynamicDataBaseManager_;                 ///< dynamic database manager
    const algo::ShuntCounterDefinitions& shuntCounters_;                           ///< Shunt counters to use
    const algo::LinesByIdDefinitions& linesByIdDefinitions_;                       ///< lines by id to use
    std::shared_ptr<ParLoads> parLoads_;                                           ///< reference to load par writer
    std::shared_ptr<ParSVarC> parSVarC_;                                           ///< reference to svarcs par writer
    std::shared_ptr<ParHvdc> parHvdc_;                                             ///< reference to hvdcs par writer
    std::shared_ptr<ParGenerator> parGenerator_;                                   ///< reference to generators par writer
    std::shared_ptr<ParDynModel> parDynModel_;                                     ///< reference to defined dynamic model par writer
  };

  /**
   * @brief Constructor
   *
   * @param def PAR file definition
   */
  explicit Par(ParDefinition&& def);

  /**
   * @brief Export PAR file
   */
  void write() const;

 private:
  ParDefinition def_;  ///< PAR file definition
};

}  // namespace outputs
}  // namespace dfl
