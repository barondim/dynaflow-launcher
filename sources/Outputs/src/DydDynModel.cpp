//
// Copyright (c) 2022, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//

#include "DydDynModel.h"

#include "Constants.h"
#include "DydCommon.h"
#include "Log.h"
#include "OutputsConstants.h"

#include <DYDMacroConnectFactory.h>
#include <DYDMacroConnectorFactory.h>
#include <DYDMacroStaticRefFactory.h>
#include <DYDMacroStaticReferenceFactory.h>

namespace dfl {
namespace outputs {

DydDynModel::DydDynModel(const algo::DynamicModelDefinitions& dynamicModelsDefinitions, const std::vector<algo::GeneratorDefinition>& gens) :
    dynamicModelsDefinitions_(dynamicModelsDefinitions) {
  for (const auto& generator : gens) {
    if (generator.isNetwork()) {
      continue;
    }
    generatorsWithDynamicModels_.insert(generator.id);
  }
}

void
DydDynModel::write(boost::shared_ptr<dynamicdata::DynamicModelsCollection>& dynamicModelsToConnect, const std::string& basename,
                   const inputs::DynamicDataBaseManager& dynamicDataBaseManager) {
  for (const auto& model : dynamicModelsDefinitions_.models) {
    auto blackBoxModel = helper::buildBlackBox(model.second.id, model.second.lib, basename + ".par", model.second.id);
    dynamicModelsToConnect->addModel(blackBoxModel);
    if (model.second.lib == common::constants::svcModelName) {
      writeSVCMacroConnector(dynamicModelsToConnect, model.second, dynamicDataBaseManager);
    } else {
      writeMacroConnector(dynamicModelsToConnect, model.second);
    }
  }
  writeMacroConnectors(dynamicModelsToConnect, dynamicDataBaseManager);
}

void
DydDynModel::writeMacroConnector(boost::shared_ptr<dynamicdata::DynamicModelsCollection>& dynamicModelsToConnect,
                                 const algo::DynamicModelDefinition& dynModel) {
  const auto& connections = dynModel.nodeConnections;

  // Here we compute the number of connections performed by macro connection, in order to generate the corresponding
  // index attributes
  std::unordered_map<std::string, std::tuple<unsigned int, unsigned int>> indexes;
  enum { INDEXES_NB_CONNECTIONS = 0, INDEXES_CURRENT_INDEX };
  for (const auto& connection : connections) {
    if (indexes.count(connection.id) > 0) {
      (std::get<INDEXES_NB_CONNECTIONS>(indexes.at(connection.id)))++;
    } else {
      indexes[connection.id] = std::make_tuple(1, 0);
    }
  }

  for (const auto& connection : connections) {
    auto macroConnect = dynamicdata::MacroConnectFactory::newMacroConnect(connection.id, dynModel.id, constants::networkModelName);
    macroConnect->setName2(connection.connectedElementId);
#if _DEBUG_
    assert(std::get<INDEXES_CURRENT_INDEX>(indexes.at(connection.id)) < std::get<INDEXES_NB_CONNECTIONS>(indexes.at(connection.id)));
#endif
    // We put index1 to 0 even in case there is only one connection, for consistency in the output file
    macroConnect->setIndex1(std::to_string(std::get<INDEXES_CURRENT_INDEX>(indexes.at(connection.id))));
    (std::get<INDEXES_CURRENT_INDEX>(indexes.at(connection.id)))++;
    dynamicModelsToConnect->addMacroConnect(macroConnect);
  }
}

void
DydDynModel::writeSVCMacroConnector(boost::shared_ptr<dynamicdata::DynamicModelsCollection>& dynamicModelsToConnect,
                                    const algo::DynamicModelDefinition& automaton, const inputs::DynamicDataBaseManager& dynamicDataBaseManager) {
  const auto& connections = automaton.nodeConnections;

  std::unordered_set<std::string> generators;
  auto it = dynamicDataBaseManager.assembling().dynamicAutomatons().find(automaton.id);
  assert(it != dynamicDataBaseManager.assembling().dynamicAutomatons().end());
  for (const auto& macroConn : it->second.macroConnects) {
    if (dynamicDataBaseManager.assembling().isSingleAssociation(macroConn.id)) {
      for (const auto& gen : dynamicDataBaseManager.assembling().getSingleAssociation(macroConn.id).generators) {
        generators.insert(gen.name);
      }
    }
  }

  unsigned idx = 1;
  for (const auto& connection : connections) {
    if (generatorsWithDynamicModels_.find(connection.connectedElementId) != generatorsWithDynamicModels_.end()) {
      auto macroConnect = dynamicdata::MacroConnectFactory::newMacroConnect(connection.id, automaton.id, connection.connectedElementId);
      macroConnect->setIndex1(std::to_string(idx));
      ++idx;
      dynamicModelsToConnect->addMacroConnect(macroConnect);
    } else if (generators.find(connection.connectedElementId) == generators.end()) {
      // U measurement
      auto macroConnect = dynamicdata::MacroConnectFactory::newMacroConnect(connection.id, automaton.id, constants::networkModelName);
      macroConnect->setName2(connection.connectedElementId);
      dynamicModelsToConnect->addMacroConnect(macroConnect);
    }
  }
}

void
DydDynModel::writeMacroConnectors(boost::shared_ptr<dynamicdata::DynamicModelsCollection>& dynamicModelsToConnect,
                                  const inputs::DynamicDataBaseManager& dynamicDataBaseManager) {
  for (const auto& macro : dynamicModelsDefinitions_.usedMacroConnections) {
    auto macroConnector = dynamicdata::MacroConnectorFactory::newMacroConnector(macro);
    for (const auto& connection : dynamicDataBaseManager.assembling().getMacroConnection(macro).connections) {
      macroConnector->addConnect(connection.var1, connection.var2);
    }
    dynamicModelsToConnect->addMacroConnector(macroConnector);
  }
}

}  // namespace outputs
}  // namespace dfl
