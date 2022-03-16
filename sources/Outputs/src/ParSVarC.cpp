//
// Copyright (c) 2022, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//

#include "ParSVarC.h"

#include "ParCommon.h"

#include <PARMacroParameterSet.h>
#include <boost/make_shared.hpp>

namespace dfl {
namespace outputs {

const std::string ParSVarC::macroParameterSetStaticCompensator_("MacroParameterSetStaticCompensator");

void
ParSVarC::write(boost::shared_ptr<parameters::ParametersSetCollection>& paramSetCollection) {
  if (!svarcsDefinitions_.empty()) {
    paramSetCollection->addMacroParameterSet(writeMacroParameterSetStaticVarCompensators());
    for (const auto& svarc : svarcsDefinitions_) {
      if (svarc.isNetwork()) {
        continue;
      }
      paramSetCollection->addParametersSet(writeStaticVarCompensator(svarc));
    }
  }
}

boost::shared_ptr<parameters::MacroParameterSet>
ParSVarC::writeMacroParameterSetStaticVarCompensators() {
  auto macro = boost::make_shared<parameters::MacroParameterSet>(macroParameterSetStaticCompensator_);

  macro->addReference(helper::buildReference("SVarC_P0Pu", "p_pu", "DOUBLE"));
  macro->addReference(helper::buildReference("SVarC_Q0Pu", "q_pu", "DOUBLE"));
  macro->addReference(helper::buildReference("SVarC_U0Pu", "v_pu", "DOUBLE"));
  macro->addReference(helper::buildReference("SVarC_UPhase0", "angle_pu", "DOUBLE"));

  return macro;
}

boost::shared_ptr<parameters::ParametersSet>
ParSVarC::writeStaticVarCompensator(const algo::StaticVarCompensatorDefinition& svarc) {
  std::size_t hashId = constants::hash(svarc.id);
  std::string hashIdStr = std::to_string(hashId);
  auto set = boost::shared_ptr<parameters::ParametersSet>(new parameters::ParametersSet(hashIdStr));

  set->addMacroParSet(boost::make_shared<parameters::MacroParSet>(macroParameterSetStaticCompensator_));
  double value;

  value = svarc.voltageSetPoint / svarc.UNom;
  set->addParameter(helper::buildParameter("SVarC_URef0Pu", value));
  set->addParameter(helper::buildParameter("SVarC_UNom", svarc.UNom));
  value = computeBPU(svarc.b0, svarc.UNom);
  set->addParameter(helper::buildParameter("SVarC_BShuntPu", value));
  value = computeBPU(svarc.bMax, svarc.UNom);
  set->addParameter(helper::buildParameter("SVarC_BMaxPu", value));
  value = computeBPU(svarc.bMin, svarc.UNom);
  set->addParameter(helper::buildParameter("SVarC_BMinPu", value));
  switch (svarc.model) {
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVMODEHANDLING:
    set->addReference(helper::buildReference("SVarC_Mode0", "regulatingMode", "INT"));
    set->addParameter(helper::buildParameter("SVarC_URefDown", svarc.USetPointMin));
    set->addParameter(helper::buildParameter("SVarC_URefUp", svarc.USetPointMax));
    set->addParameter(helper::buildParameter("SVarC_UThresholdDown", svarc.UMinActivation));
    set->addParameter(helper::buildParameter("SVarC_UThresholdUp", svarc.UMaxActivation));
    set->addParameter(helper::buildParameter("SVarC_tThresholdDown", static_cast<double>(svarcThresholdDown_)));
    set->addParameter(helper::buildParameter("SVarC_tThresholdUp", static_cast<double>(svarcThresholdUp_)));
    break;
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVREMOTE:
    set->addParameter(helper::buildParameter("SVarC_UNomRemote", svarc.UNomRemote));
    break;
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVREMOTEMODEHANDLING:
    set->addParameter(helper::buildParameter("SVarC_UNomRemote", svarc.UNomRemote));
    set->addReference(helper::buildReference("SVarC_Mode0", "regulatingMode", "INT"));
    set->addParameter(helper::buildParameter("SVarC_URefDown", svarc.USetPointMin));
    set->addParameter(helper::buildParameter("SVarC_URefUp", svarc.USetPointMax));
    set->addParameter(helper::buildParameter("SVarC_UThresholdDown", svarc.UMinActivation));
    set->addParameter(helper::buildParameter("SVarC_UThresholdUp", svarc.UMaxActivation));
    set->addParameter(helper::buildParameter("SVarC_tThresholdDown", static_cast<double>(svarcThresholdDown_)));
    set->addParameter(helper::buildParameter("SVarC_tThresholdUp", static_cast<double>(svarcThresholdUp_)));
    break;
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVPROP:
    value = svarc.slope * Sb_ / svarc.UNom;
    set->addParameter(helper::buildParameter("SVarC_LambdaPu", value));
    break;
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVPROPMODEHANDLING:
    value = svarc.slope * Sb_ / svarc.UNom;
    set->addParameter(helper::buildParameter("SVarC_LambdaPu", value));
    set->addReference(helper::buildReference("SVarC_Mode0", "regulatingMode", "INT"));
    set->addParameter(helper::buildParameter("SVarC_URefDown", svarc.USetPointMin));
    set->addParameter(helper::buildParameter("SVarC_URefUp", svarc.USetPointMax));
    set->addParameter(helper::buildParameter("SVarC_UThresholdDown", svarc.UMinActivation));
    set->addParameter(helper::buildParameter("SVarC_UThresholdUp", svarc.UMaxActivation));
    set->addParameter(helper::buildParameter("SVarC_tThresholdDown", static_cast<double>(svarcThresholdDown_)));
    set->addParameter(helper::buildParameter("SVarC_tThresholdUp", static_cast<double>(svarcThresholdUp_)));
    break;
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVPROPREMOTE:
    value = svarc.slope * Sb_ / svarc.UNom;
    set->addParameter(helper::buildParameter("SVarC_LambdaPu", value));
    set->addParameter(helper::buildParameter("SVarC_UNomRemote", svarc.UNomRemote));
    break;
  case algo::StaticVarCompensatorDefinition::ModelType::SVARCPVPROPREMOTEMODEHANDLING:
    value = svarc.slope * Sb_ / svarc.UNom;
    set->addParameter(helper::buildParameter("SVarC_LambdaPu", value));
    set->addParameter(helper::buildParameter("SVarC_UNomRemote", svarc.UNomRemote));
    set->addReference(helper::buildReference("SVarC_Mode0", "regulatingMode", "INT"));
    set->addParameter(helper::buildParameter("SVarC_URefDown", svarc.USetPointMin));
    set->addParameter(helper::buildParameter("SVarC_URefUp", svarc.USetPointMax));
    set->addParameter(helper::buildParameter("SVarC_UThresholdDown", svarc.UMinActivation));
    set->addParameter(helper::buildParameter("SVarC_UThresholdUp", svarc.UMaxActivation));
    set->addParameter(helper::buildParameter("SVarC_tThresholdDown", static_cast<double>(svarcThresholdDown_)));
    set->addParameter(helper::buildParameter("SVarC_tThresholdUp", static_cast<double>(svarcThresholdUp_)));
    break;
  default:
    break;
  }

  return set;
}

}  // namespace outputs
}  // namespace dfl
