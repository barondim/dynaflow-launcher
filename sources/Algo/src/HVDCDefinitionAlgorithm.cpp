//
// Copyright (c) 2022, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//

#include "HVDCDefinitionAlgorithm.h"

#include "Constants.h"
#include "Log.h"

#include <DYNCommon.h>
#include <DYNTimer.h>

namespace dfl {
namespace algo {

HVDCDefinitionAlgorithm::HVDCDefinitionAlgorithm(HVDCLineDefinitions &hvdcLinesDefinitions,
                                                 const inputs::NetworkManager::BusMapRegulating &busesToNumberOfRegulationMap, bool infiniteReactiveLimits,
                                                 const std::unordered_set<std::shared_ptr<inputs::Converter>> &converters,
                                                 const inputs::DynamicDataBaseManager &manager)
    : hvdcLinesDefinitions_(hvdcLinesDefinitions), busesToNumberOfRegulationMap_(busesToNumberOfRegulationMap),
      infiniteReactiveLimits_(infiniteReactiveLimits) {
  std::transform(converters.begin(), converters.end(), std::inserter(vscConverters_, vscConverters_.begin()),
                 [](const std::shared_ptr<inputs::Converter> &converter) { return std::make_pair(converter->busId, converter); });
  for (const auto &automaton : manager.assembling().dynamicAutomatons()) {
    if (automaton.second.lib == dfl::common::constants::svcModelName) {
      for (const auto &macroConn : automaton.second.macroConnects) {
        if (manager.assembling().isSingleAssociation(macroConn.id)) {
          const auto &assoc = manager.assembling().getSingleAssociation(macroConn.id);
          if (assoc.hvdcLine) {
            hvdcLinesInSVC.insert(assoc.hvdcLine.get().name);
          }
        }
      }
    }
  }
}

auto HVDCDefinitionAlgorithm::computeModelVSC(const inputs::HvdcLine &hvdcline, HVDCDefinition::Position position,
                                              HVDCDefinition::HVDCModel multipleVSCInfiniteReactive, HVDCDefinition::HVDCModel multipleVSCFiniteReactive,
                                              HVDCDefinition::HVDCModel oneVSCInfiniteReactive, HVDCDefinition::HVDCModel oneVSCFiniteReactive) const
    -> HVDCModelDefinition {
  auto regulation = dfl::inputs::NetworkManager::NbOfRegulating::ONE;
  switch (position) {
  case HVDCDefinition::Position::FIRST_IN_MAIN_COMPONENT: {
    auto found = busesToNumberOfRegulationMap_.find(hvdcline.converter1->busId);
    if (found != busesToNumberOfRegulationMap_.end())
      regulation = found->second;
    break;
  }
  case HVDCDefinition::Position::SECOND_IN_MAIN_COMPONENT: {
    auto found = busesToNumberOfRegulationMap_.find(hvdcline.converter2->busId);
    if (found != busesToNumberOfRegulationMap_.end())
      regulation = found->second;
    break;
  }
  case HVDCDefinition::Position::BOTH_IN_MAIN_COMPONENT: {
    auto found1 = busesToNumberOfRegulationMap_.find(hvdcline.converter1->busId);
    auto found2 = busesToNumberOfRegulationMap_.find(hvdcline.converter2->busId);
    if ((found1 != busesToNumberOfRegulationMap_.end() && found1->second == dfl::inputs::NetworkManager::NbOfRegulating::MULTIPLES) ||
        (found2 != busesToNumberOfRegulationMap_.end() && found2->second == dfl::inputs::NetworkManager::NbOfRegulating::MULTIPLES))
      regulation = dfl::inputs::NetworkManager::NbOfRegulating::MULTIPLES;
    break;
  }
  default:  // impossible case by definition of the enum
    break;
  }

  if (regulation == dfl::inputs::NetworkManager::NbOfRegulating::MULTIPLES) {
    return HVDCModelDefinition{infiniteReactiveLimits_ ? multipleVSCInfiniteReactive : multipleVSCFiniteReactive};
  } else {
    return HVDCModelDefinition{infiniteReactiveLimits_ ? oneVSCInfiniteReactive : oneVSCFiniteReactive};
  }
}

auto HVDCDefinitionAlgorithm::computeModel(const inputs::HvdcLine &hvdcline, HVDCDefinition::Position position, inputs::HvdcLine::ConverterType type) const
    -> HVDCModelDefinition {
  const bool isInSVC = hvdcLinesInSVC.find(hvdcline.id) != hvdcLinesInSVC.end();
  if (position == HVDCDefinition::Position::BOTH_IN_MAIN_COMPONENT) {
    if (type == inputs::HvdcLine::ConverterType::LCC) {
      return HVDCModelDefinition{infiniteReactiveLimits_ ? HVDCDefinition::HVDCModel::HvdcPTanPhi : HVDCDefinition::HVDCModel::HvdcPTanPhiDiagramPQ};
    } else {
      const bool hvdcAngleDroopActivePowerControlIsEnabled = hvdcline.activePowerControl.has_value();
      if (!hvdcAngleDroopActivePowerControlIsEnabled) {
        if (isInSVC)
          return HVDCModelDefinition{infiniteReactiveLimits_ ? HVDCDefinition::HVDCModel::HvdcPVRpcl2Side1
                                                             : HVDCDefinition::HVDCModel::HvdcPVDiagramPQRpcl2Side1};
        else
          return computeModelVSC(hvdcline, position, HVDCDefinition::HVDCModel::HvdcPQProp, HVDCDefinition::HVDCModel::HvdcPQPropDiagramPQ,
                                 HVDCDefinition::HVDCModel::HvdcPV, HVDCDefinition::HVDCModel::HvdcPVDiagramPQ);
      } else {
        if (isInSVC)
          return HVDCModelDefinition{infiniteReactiveLimits_ ? HVDCDefinition::HVDCModel::HvdcPVEmulationSetRpcl2Side1
                                                             : HVDCDefinition::HVDCModel::HvdcPVDiagramPQEmulationSetRpcl2Side1};
        else
          return computeModelVSC(hvdcline, position, HVDCDefinition::HVDCModel::HvdcPQPropEmulationSet,
                                 HVDCDefinition::HVDCModel::HvdcPQPropDiagramPQEmulationSet, HVDCDefinition::HVDCModel::HvdcPVEmulationSet,
                                 HVDCDefinition::HVDCModel::HvdcPVDiagramPQEmulationSet);
      }
    }
  } else {
    // case only one of them is in the main connex component
    if (type == inputs::HvdcLine::ConverterType::LCC) {
      return HVDCModelDefinition{infiniteReactiveLimits_ ? HVDCDefinition::HVDCModel::HvdcPTanPhiDangling
                                                         : HVDCDefinition::HVDCModel::HvdcPTanPhiDanglingDiagramPQ};
    } else {
      if (isInSVC)
        return HVDCModelDefinition{infiniteReactiveLimits_ ? HVDCDefinition::HVDCModel::HvdcPVDanglingRpcl2Side1
                                                           : HVDCDefinition::HVDCModel::HvdcPVDanglingDiagramPQRpcl2Side1};
      else
        return computeModelVSC(hvdcline, position, HVDCDefinition::HVDCModel::HvdcPQPropDangling, HVDCDefinition::HVDCModel::HvdcPQPropDanglingDiagramPQ,
                               HVDCDefinition::HVDCModel::HvdcPVDangling, HVDCDefinition::HVDCModel::HvdcPVDanglingDiagramPQ);
    }
  }
}

std::pair<std::reference_wrapper<HVDCDefinition>, bool> HVDCDefinitionAlgorithm::getOrCreateHvdcLineDefinition(const inputs::HvdcLine &hvdcLine) {
  auto &hvdcLines = hvdcLinesDefinitions_.hvdcLines;
  auto it = hvdcLines.find(hvdcLine.id);
  bool alreadyInserted = it != hvdcLines.end();
  if (alreadyInserted) {
    return {std::ref(it->second), alreadyInserted};
  } else {
    std::array<double, 2> powerFactors{0., 0.};
    boost::optional<VSCDefinition> def1;
    boost::optional<VSCDefinition> def2;
    boost::optional<bool> voltageRegulation1;
    boost::optional<bool> voltageRegulation2;
    if (hvdcLine.converterType == inputs::HvdcLine::ConverterType::LCC) {
      auto converterLCC1 = std::dynamic_pointer_cast<inputs::LCCConverter>(hvdcLine.converter1);
      auto converterLCC2 = std::dynamic_pointer_cast<inputs::LCCConverter>(hvdcLine.converter2);
      powerFactors.at(0) = converterLCC1->powerFactor;
      powerFactors.at(1) = converterLCC2->powerFactor;
    } else {
      auto converterVSC1 = std::dynamic_pointer_cast<inputs::VSCConverter>(hvdcLine.converter1);
      auto converterVSC2 = std::dynamic_pointer_cast<inputs::VSCConverter>(hvdcLine.converter2);
      def1 = VSCDefinition(converterVSC1->converterId, converterVSC1->qMax, converterVSC1->qMin, converterVSC1->q, hvdcLine.pMax, converterVSC1->points);
      def2 = VSCDefinition(converterVSC2->converterId, converterVSC2->qMax, converterVSC2->qMin, converterVSC2->q, hvdcLine.pMax, converterVSC2->points);
      voltageRegulation1 = converterVSC1->voltageRegulationOn;
      voltageRegulation2 = converterVSC2->voltageRegulationOn;
    }

    boost::optional<double> droop = (hvdcLine.activePowerControl) ? hvdcLine.activePowerControl->droop : boost::optional<double>();
    boost::optional<double> p0 = (hvdcLine.activePowerControl) ? hvdcLine.activePowerControl->p0 : boost::optional<double>();
    HVDCDefinition createdHvdcLine(hvdcLine.id, hvdcLine.converterType, hvdcLine.converter1->converterId, hvdcLine.converter1->busId, voltageRegulation1,
                                   hvdcLine.converter2->converterId, hvdcLine.converter2->busId, voltageRegulation2,
                                   HVDCDefinition::Position::BOTH_IN_MAIN_COMPONENT, HVDCDefinition::HVDCModel::HvdcPTanPhi, powerFactors, hvdcLine.pMax, def1,
                                   def2, droop, p0, hvdcLine.isConverter1Rectifier, hvdcLine.vdcNom, hvdcLine.pSetPoint, hvdcLine.rdc, hvdcLine.lossFactors);
    auto pair = hvdcLines.emplace(hvdcLine.id, createdHvdcLine);
    return {std::ref(pair.first->second), alreadyInserted};
  }
}

void HVDCDefinitionAlgorithm::operator()(const NodePtr &node, std::shared_ptr<AlgorithmsResults> &) {
#if defined(_DEBUG_) || defined(PRINT_TIMERS)
  DYN::Timer timer("DFL::HVDCDefinitionAlgorithm::operator()");
#endif
  for (const auto &converterPtr : node->converters) {
    auto converter = converterPtr.lock();
    const auto &hvdcLine = converter->hvdcLine;
    auto hvdcLineDefPair = getOrCreateHvdcLineDefinition(*hvdcLine);
    auto &hvdcLineDefinition = hvdcLineDefPair.first.get();

    if (hvdcLineDefPair.second) {
      // If we meet twice the same HVDC line with two different converters in nodes, it means that both extremities are
      // in the component: it is assumed that the algorithm is performed on main connex component
      hvdcLineDefinition.position = HVDCDefinition::Position::BOTH_IN_MAIN_COMPONENT;
    } else if (converter->converterId == hvdcLine->converter1->converterId) {
      hvdcLineDefinition.position = HVDCDefinition::Position::FIRST_IN_MAIN_COMPONENT;
    } else if (converter->converterId == hvdcLine->converter2->converterId) {
      hvdcLineDefinition.position = HVDCDefinition::Position::SECOND_IN_MAIN_COMPONENT;
    } else {
      LOG(warn, HvdcLineBadInitialization, hvdcLine->id);
      continue;
    }

    auto modelDef = computeModel(*hvdcLine, hvdcLineDefinition.position, hvdcLineDefinition.converterType);
    hvdcLineDefinition.model = modelDef.model;
  }
}

}  // namespace algo
}  // namespace dfl
