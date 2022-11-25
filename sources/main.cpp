//
// Copyright (c) 2020, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0

#include "Configuration.h"
#include "Context.h"
#include "Contingencies.h"
#include "Log.h"
#include "Options.h"
#include "SimulationParams.h"
#include "gitversion_dfl.h"
#include "version.h"

#include <DYNError.h>
#include <DYNInitXml.h>
#include <DYNIoDico.h>
#include <DYNMPIContext.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <cstdlib>
#include <sstream>

static inline std::string
getMandatoryEnvVar(const std::string& key) {
  char* var = getenv(key.c_str());
  if (var != NULL) {
    return std::string(var);
  } else {
    // we cannot use dictionnary errors since they may not be initialized yet
    throw Error(EnvVariableMissing, key);
  }
}

static void
initializeDynawo(const std::string& locale) {
  DYN::IoDicos& dicos = DYN::IoDicos::instance();
  dicos.addPath(getMandatoryEnvVar("DYNAWO_RESOURCES_DIR"));
  dicos.addDico("ERROR", "DYNError", locale);
  dicos.addDico("TIMELINE", "DYNTimeline", locale);
  dicos.addDico("CONSTRAINT", "DYNConstraint", locale);
  dicos.addDico("LOG", "DYNLog", locale);
  dicos.addDico("DFLLOG", "DFLLog", locale);
  dicos.addDico("DFLERROR", "DFLError", locale);
}

static inline double
elapsed(const std::chrono::steady_clock::time_point& timePoint) {
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timePoint);
  return static_cast<double>(duration.count()) / 1000;  // To have the time in seconds as a double
}

static inline boost::filesystem::path
getOutputDir(const boost::filesystem::path& configFilepath) {
  boost::property_tree::ptree tree;
  boost::property_tree::read_json(configFilepath.generic_string(), tree);
  boost::filesystem::path path = boost::filesystem::current_path();
  const auto optionValue = tree.get_child("dfl-config").get_child_optional("OutputDir");
  if (optionValue.is_initialized()) {
    path = optionValue->get_value<std::string>();
  }
  return path;
}

static boost::shared_ptr<dfl::Context>
buildContext(dfl::inputs::SimulationParams const& params) {
  boost::filesystem::path configPath(params.runtimeConfig->configPath);
  dfl::inputs::Configuration config(configPath, params.simulationKind);

  dfl::Context::ContextDef def{config.getStartingPointMode(),
                               params.simulationKind,
                               params.networkFilePath,
                               config.settingFilePath(),
                               config.assemblingFilePath(),
                               params.runtimeConfig->contingenciesFilePath,
                               params.runtimeConfig->dynawoLogLevel,
                               params.parametersDirPath,
                               params.resourcesDirPath,
                               params.locale};

  boost::shared_ptr<dfl::Context> context = boost::shared_ptr<dfl::Context>(new dfl::Context(def, config));
  return context;
}

static void
execSimulation(boost::shared_ptr<dfl::Context> context, dfl::inputs::SimulationParams const& params) {
  std::string simuName =
      params.simulationKind == dfl::inputs::Configuration::SimulationKind::SECURITY_ANALYSIS ? "Security analysis simulation" : "Steady state simulation";
  DYNAlgorithms::mpi::Context& mpiContext = DYNAlgorithms::mpi::context();
  try {
    if (!context->process()) {
      LOG(info, InitEnd, elapsed(params.timeStart));
      throw Error(ContextProcessError, context->basename());
    }
    auto timeFilesStart = std::chrono::steady_clock::now();
    context->exportOutputs();
    LOG(info, FilesEnd, elapsed(timeFilesStart));

    DYNAlgorithms::mpi::Context::sync();

    auto timeSimuStart = std::chrono::steady_clock::now();
    if (params.simulationKind == dfl::inputs::Configuration::SimulationKind::SECURITY_ANALYSIS) {
      context->execute();
      context->exportResults(true);
    } else {
      // For steady-state calculation, only the root process is allowed to perform the simulation.
      if (mpiContext.isRootProc()) {
        context->execute();
        context->exportResults(true);
      }
    }

    if (mpiContext.isRootProc()) {
      LOG(info, SimulationEnded, context->basename(), elapsed(timeSimuStart));
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      LOG(info, DFLEnded, context->basename(), elapsed(params.timeStart));
    }
  } catch (DYN::Error& e) {
    context->exportResults(false);
    if (mpiContext.isRootProc()) {
      std::cerr << simuName << " failed" << std::endl;
      std::cerr << "Dynawo: " << e.what() << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Simulation failed" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Dynawo: " << e.what() << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    exit(EXIT_FAILURE);
  } catch (DYN::MessageError& e) {
    context->exportResults(false);
    if (mpiContext.isRootProc()) {
      std::cerr << simuName << " failed" << std::endl;
      std::cerr << "Dynawo: " << e.what() << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Simulation failed" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Dynawo: " << e.what() << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    exit(EXIT_FAILURE);
  } catch (std::exception& e) {
    context->exportResults(false);
    if (mpiContext.isRootProc()) {
      std::cerr << simuName << " failed" << std::endl;
      std::cerr << e.what() << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Simulation failed" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << e.what() << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    exit(EXIT_FAILURE);
  } catch (...) {
    context->exportResults(false);
    if (mpiContext.isRootProc()) {
      std::cerr << simuName << " failed" << std::endl;
      std::cerr << "Unknown error" << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Simulation failed" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << "Unknown error" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    exit(EXIT_FAILURE);
  }
}

int
main(int argc, char* argv[]) {
  DYNAlgorithms::mpi::Context& mpiContext = DYNAlgorithms::mpi::context();  // MUST be at the beginning to initialize the instance
  DYN::InitXerces xerces;
  DYN::InitLibXml2 libxml2;
  auto timeStart = std::chrono::steady_clock::now();
  DYN::Trace::init();
  dfl::common::Options options;

  const auto userRequest = options.parse(argc, argv);

  switch (userRequest) {
  case dfl::common::Options::Request::HELP:
  case dfl::common::Options::Request::ERROR:
    if (mpiContext.isRootProc()) {
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag) << options.desc() << DYN::Trace::endline;
    }
    return EXIT_SUCCESS;
  case dfl::common::Options::Request::VERSION:
    if (mpiContext.isRootProc()) {
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag)
          << DYNAFLOW_LAUNCHER_VERSION_STRING << " (rev:" << DYNAFLOW_LAUNCHER_GIT_BRANCH << "-" << DYNAFLOW_LAUNCHER_GIT_HASH << ")" << DYN::Trace::endline;
    }
    return EXIT_SUCCESS;
  default:
    break;
  }

  auto& runtimeConfig = options.config();

  std::string root;
  std::string locale;
  boost::filesystem::path resourcesDir;

  boost::filesystem::path outputDir = getOutputDir(runtimeConfig.configPath);

  try {
    if (mpiContext.isRootProc()) {
      dfl::common::Log::init(options, outputDir.generic_string());
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag)
          << " " << runtimeConfig.programName << " v" << DYNAFLOW_LAUNCHER_VERSION_STRING << DYN::Trace::endline;
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag)
          << " "
          << " revision " << DYNAFLOW_LAUNCHER_GIT_BRANCH << "-" << DYNAFLOW_LAUNCHER_GIT_HASH << DYN::Trace::endline;
      DYN::Trace::info(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }

    resourcesDir = boost::filesystem::path(getMandatoryEnvVar("DYNAWO_RESOURCES_DIR"));
    root = getMandatoryEnvVar("DYNAFLOW_LAUNCHER_INSTALL");
    locale = getMandatoryEnvVar("DYNAFLOW_LAUNCHER_LOCALE");

    initializeDynawo(locale);
  } catch (DYN::Error& e) {
    if (mpiContext.isRootProc()) {
      std::cerr << "Initialization failed: " << e.what() << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " Initialization failed: " << e.what() << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    return EXIT_FAILURE;
  } catch (DYN::MessageError& e) {
    if (mpiContext.isRootProc()) {
      std::cerr << "Initialization failed: " << e.what() << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " Initialization failed: " << e.what() << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    return EXIT_FAILURE;
  } catch (std::exception& e) {
    if (mpiContext.isRootProc()) {
      std::cerr << "Initialization failed: " << e.what() << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " Initialization failed: " << e.what() << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    return EXIT_FAILURE;
  } catch (...) {
    if (mpiContext.isRootProc()) {
      std::cerr << "Initialization failed" << std::endl;
      std::cerr << "Unknown error" << std::endl;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " Initialization failed" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " Unknown error" << DYN::Trace::endline;
      DYN::Trace::error(dfl::common::Log::dynaflowLauncherLogTag) << " ============================================================ " << DYN::Trace::endline;
    }
    return EXIT_FAILURE;
  }

  if (!boost::filesystem::exists(boost::filesystem::path(runtimeConfig.networkFilePath))) {
    throw Error(NetworkFileNotFound, runtimeConfig.networkFilePath);
  }
  if (!runtimeConfig.contingenciesFilePath.empty() && !boost::filesystem::exists(boost::filesystem::path(runtimeConfig.contingenciesFilePath))) {
    throw Error(ContingenciesFileNotFound, runtimeConfig.contingenciesFilePath);
  }
  if (userRequest == dfl::common::Options::Request::RUN_SIMULATION_NSA && runtimeConfig.contingenciesFilePath.empty()) {
    throw Error(ContingenciesFileNotFound, runtimeConfig.contingenciesFilePath);
  }

  switch (userRequest) {
  case dfl::common::Options::Request::RUN_SIMULATION_N:
    LOG(info, SteadyStateInfo, runtimeConfig.networkFilePath, runtimeConfig.configPath);
    break;
  case dfl::common::Options::Request::RUN_SIMULATION_SA:
    LOG(info, SecurityAnalysisInfo, runtimeConfig.networkFilePath, runtimeConfig.contingenciesFilePath, runtimeConfig.configPath);
    break;
  case dfl::common::Options::Request::RUN_SIMULATION_NSA:
    LOG(info, SteadyStateAndSecurityAnalysisInfo, runtimeConfig.networkFilePath, runtimeConfig.contingenciesFilePath, runtimeConfig.configPath);
    break;
  default:
    break;
  }

  boost::filesystem::path parFilesDir(root);
  parFilesDir.append("etc");

  dfl::inputs::SimulationParams params;
  params.runtimeConfig = &runtimeConfig;
  params.timeStart = timeStart;
  params.parametersDirPath = parFilesDir;
  params.resourcesDirPath = resourcesDir;
  params.networkFilePath = runtimeConfig.networkFilePath;
  params.locale = locale;

  if (userRequest == dfl::common::Options::Request::RUN_SIMULATION_N || userRequest == dfl::common::Options::Request::RUN_SIMULATION_NSA) {
    params.simulationKind = dfl::inputs::Configuration::SimulationKind::STEADY_STATE_CALCULATION;

    boost::shared_ptr<dfl::Context> context = buildContext(params);
    execSimulation(context, params);
  }

  DYNAlgorithms::mpi::Context::sync();

  if (userRequest == dfl::common::Options::Request::RUN_SIMULATION_SA || userRequest == dfl::common::Options::Request::RUN_SIMULATION_NSA) {
    params.simulationKind = dfl::inputs::Configuration::SimulationKind::SECURITY_ANALYSIS;

    if (userRequest == dfl::common::Options::Request::RUN_SIMULATION_NSA) {
      params.networkFilePath = absolute("outputs/finalState/outputIIDM.xml", outputDir.string());
    }
    boost::shared_ptr<dfl::Context> context = buildContext(params);
    execSimulation(context, params);
  }

  return EXIT_SUCCESS;
}
