//
// Copyright (c) 2023, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//

#include "DydVRRemote.h"

#include "DydCommon.h"
#include "OutputsConstants.h"

#include <DYDMacroConnectFactory.h>
#include <DYDMacroConnectorFactory.h>
#include <set>

namespace dfl {
namespace outputs {

void DydVRRemote::writeVRRemotes(boost::shared_ptr<dynamicdata::DynamicModelsCollection> &dynamicModelsToConnect, const std::string &basename) {
  writeMacroConnector(dynamicModelsToConnect);
  writeConnections(dynamicModelsToConnect, basename);
}

void DydVRRemote::writeMacroConnector(boost::shared_ptr<dynamicdata::DynamicModelsCollection> &dynamicModelsToConnect) {
  for (auto it = generatorDefinitions_.cbegin(); it != generatorDefinitions_.cend(); ++it) {
    if (it->isRegulatingLocallyWithOthers()) {
      auto connector = dynamicdata::MacroConnectorFactory::newMacroConnector(macroConnectorGenVRRemoteName_);
      connector->addConnect("generator_NQ", "vrremote_NQ");
      connector->addConnect("generator_limUQUp", "vrremote_limUQUp_@INDEX@_");
      connector->addConnect("generator_limUQDown", "vrremote_limUQDown_@INDEX@_");
      dynamicModelsToConnect->addMacroConnector(connector);
      break;
    }
  }
  for (const auto &keyValue : hvdcDefinitions_.hvdcLines) {
    algo::HVDCDefinition hvdcLine = keyValue.second;
    if (hvdcLine.hasPQPropModel()) {
      auto connector = dynamicdata::MacroConnectorFactory::newMacroConnector(macroConnectorHvdcVRRemoteSide1Name_);
      connector->addConnect("hvdc_NQ1", "vrremote_NQ");
      connector->addConnect("hvdc_limUQUp1", "vrremote_limUQUp_@INDEX@_");
      connector->addConnect("hvdc_limUQDown1", "vrremote_limUQDown_@INDEX@_");
      dynamicModelsToConnect->addMacroConnector(connector);
      connector = dynamicdata::MacroConnectorFactory::newMacroConnector(macroConnectorHvdcVRRemoteSide2Name_);
      connector->addConnect("hvdc_NQ2", "vrremote_NQ");
      connector->addConnect("hvdc_limUQUp2", "vrremote_limUQUp_@INDEX@_");
      connector->addConnect("hvdc_limUQDown2", "vrremote_limUQDown_@INDEX@_");
      dynamicModelsToConnect->addMacroConnector(connector);
      break;
    }
  }
}

void DydVRRemote::writeConnections(boost::shared_ptr<dynamicdata::DynamicModelsCollection> &dynamicModelsToConnect, const std::string &basename) {
  std::map<std::string, unsigned int> modelBusIdToNumber;
  for (auto it = generatorDefinitions_.cbegin(); it != generatorDefinitions_.cend(); ++it) {
    if (it->isRegulatingLocallyWithOthers()) {
      assert(busesToNumberOfRegulationMap_.find(it->regulatedBusId) != busesToNumberOfRegulationMap_.end() &&
             busesToNumberOfRegulationMap_.find(it->regulatedBusId)->second == dfl::inputs::NetworkManager::NbOfRegulating::MULTIPLES);
      std::string modelNQId = constants::modelSignalNQprefix_ + it->regulatedBusId;
      auto connection = dynamicdata::MacroConnectFactory::newMacroConnect(macroConnectorGenVRRemoteName_, it->id, modelNQId);
      connection->setIndex2(std::to_string(modelBusIdToNumber[it->regulatedBusId]));
      ++modelBusIdToNumber[it->regulatedBusId];
      dynamicModelsToConnect->addMacroConnect(connection);
    }
  }

  for (const auto &keyValue : hvdcDefinitions_.hvdcLines) {
    algo::HVDCDefinition hvdcLine = keyValue.second;
    if (hvdcLine.hasPQPropModel()) {
      const algo::HVDCDefinition::BusId &busId1 =
          (hvdcLine.position == algo::HVDCDefinition::Position::SECOND_IN_MAIN_COMPONENT) ? hvdcLine.converter2BusId : hvdcLine.converter1BusId;
      std::string modelNQId = constants::modelSignalNQprefix_ + busId1;
      auto connection = dynamicdata::MacroConnectFactory::newMacroConnect(macroConnectorHvdcVRRemoteSide1Name_, hvdcLine.id, modelNQId);
      connection->setIndex2(std::to_string(modelBusIdToNumber[busId1]));
      ++modelBusIdToNumber[busId1];
      dynamicModelsToConnect->addMacroConnect(connection);

      if (hvdcLine.position == algo::HVDCDefinition::Position::BOTH_IN_MAIN_COMPONENT) {
        std::string modelNQId2 = constants::modelSignalNQprefix_ + hvdcLine.converter2BusId;
        auto connectionSide2 = dynamicdata::MacroConnectFactory::newMacroConnect(macroConnectorHvdcVRRemoteSide2Name_, hvdcLine.id, modelNQId2);
        connectionSide2->setIndex2(std::to_string(modelBusIdToNumber[hvdcLine.converter2BusId]));
        ++modelBusIdToNumber[hvdcLine.converter2BusId];
        dynamicModelsToConnect->addMacroConnect(connectionSide2);
      }
    }
  }
  for (const auto &busId : modelBusIdToNumber) {
    if (busId.second > 0) {
      std::string id = constants::modelSignalNQprefix_ + busId.first;
      boost::shared_ptr<dynamicdata::BlackBoxModel> blackBoxModelVRRemote = helper::buildBlackBox(id, "VRRemote", basename + ".par", id);
      dynamicModelsToConnect->addModel(blackBoxModelVRRemote);
      dynamicModelsToConnect->addConnect(id, "vrremote_URegulatedPu", constants::networkModelName, busId.first + "_Upu_value");
    }
  }
}

}  // namespace outputs
}  // namespace dfl
