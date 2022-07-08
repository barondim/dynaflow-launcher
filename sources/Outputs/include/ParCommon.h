//
// Copyright (c) 2021, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//

/**
 * @file  ParCommon.h
 *
 * @brief Dynaflow launcher common methods for handling Parameters
 *
 */

#pragma once

#include "Constants.h"

#include <PARParameter.h>
#include <PARParameterFactory.h>
#include <PARParametersSetCollection.h>
#include <PARParametersSetCollectionFactory.h>
#include <PARReference.h>
#include <PARReferenceFactory.h>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

namespace dfl {
namespace outputs {

namespace helper {
/**
 * @brief Helper function to build a Dynawo parameter
 *
 * @param name the parameter name
 * @param value the value of the parameter
 *
 * @return Dynawo parameter with this name and value
 */
template<class T>
boost::shared_ptr<parameters::Parameter>
buildParameter(const std::string& name, const T& value) {
  return parameters::ParameterFactory::newParameter(name, value);
}

/**
 * @brief Helper function to build a Dynawo reference
 *
 * @param name name of the reference
 * @param origName reference's origin name
 * @param type reference's type
 * @param componentId component where the reference should be found
 * @return the new reference
 */
inline boost::shared_ptr<parameters::Reference>
buildReference(const std::string& name, const std::string& origName, const std::string& type, const boost::optional<std::string>& componentId = {}) {
  auto ref = parameters::ReferenceFactory::newReference(name);
  ref->setOrigData("IIDM");
  ref->setOrigName(origName);
  ref->setType(type);
  if (componentId.is_initialized()) {
    ref->setComponentId(componentId.value());
  }

  return ref;
}

/**
 * @brief create a the Macro Parameter Set Id object
 *
 * @param modelType string to identify the macro parameter
 * @return a the Macro Parameter Set Id
 */
inline std::string
getMacroParameterSetId(const std::string& modelType) {
  return "macro_" + modelType;
}

/**
 * @brief Helper function to build a Dynawo macro parameter set for vrremote
 *
 * @param modelType string to identify the macro parameter
 * @return a Dynawo macro parameter set for vrremote
 */
inline boost::shared_ptr<parameters::MacroParameterSet>
buildMacroParameterSetVRRemote(const std::string& modelType) {
  boost::shared_ptr<parameters::MacroParameterSet> macroParameterSet =
      boost::shared_ptr<parameters::MacroParameterSet>(new parameters::MacroParameterSet(modelType));
  if (modelType == getMacroParameterSetId(constants::remoteVControlParId + "_vr")) {
    macroParameterSet->addParameter(buildParameter("vrremote_Gain", 1.));
    macroParameterSet->addParameter(buildParameter("vrremote_tIntegral", 1.));
  }
  return macroParameterSet;
}

/**
 * @brief Write remote voltage regulators parameter set
 *
 * @param busId the bus id to use
 * @param elementId the element id to use (generator or VSC converter)
 *
 * @returns the parameter set
 */
inline boost::shared_ptr<parameters::ParametersSet>
writeVRRemote(const std::string& busId, const std::string& elementId) {
  auto set = boost::shared_ptr<parameters::ParametersSet>(new parameters::ParametersSet("Model_Signal_NQ_" + busId));
  set->addReference(buildReference("vrremote_U0", "targetV", "DOUBLE", elementId));
  set->addReference(buildReference("vrremote_URef0", "targetV", "DOUBLE", elementId));
  set->addMacroParSet(boost::shared_ptr<parameters::MacroParSet>(new parameters::MacroParSet(getMacroParameterSetId(constants::remoteVControlParId + "_vr"))));
  return set;
}

}  // namespace helper

}  // namespace outputs
}  // namespace dfl