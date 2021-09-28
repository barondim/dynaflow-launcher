#!/bin/bash
INSTALL=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export DYNAWO_HOME=$INSTALL/external/dynawo
export LD_LIBRARY_PATH=$INSTALL/lib64:$DYNAWO_HOME/lib:$LD_LIBRARY_PATH
export IIDM_XML_XSD_PATH=$DYNAWO_HOME/share/iidm/xsd
export DYNAFLOW_LAUNCHER_LOCALE=en_GB
export DYNAFLOW_LAUNCHER_INSTALL=$INSTALL
export DYNAWO_RESOURCES_DIR=$DYNAWO_HOME/share:$DYNAWO_HOME/share/xsd
export DYNAWO_DDB_DIR=$DYNAWO_HOME/ddb
export DYNAWO_IIDM_EXTENSION=$DYNAWO_HOME/lib/libdynawo_DataInterfaceIIDMExtension.so
export DYNAWO_LIBIIDM_EXTENSIONS=$DYNAWO_HOME/lib
export DYNAWO_INSTALL_DIR=$DYNAWO_HOME
export DYNAFLOW_LAUNCHER_XSD=$INSTALL/etc/xsd
export DYNAFLOW_LAUNCHER_LIBRARIES=$DYNAWO_DDB_DIR

export DYNAWO_ALGORITHMS_LOCALE=en_GB

$INSTALL/bin/DynaFlowLauncher $@
