// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <notify.h>
#include <string>
#include <vector>
#include "fusion/config/gefConfigUtil.h"
#include "fusion/rasterfuse/PackgenTraverser.h"
#include "fusion/rasterfuse/ImageryPackgenTraverser.h"
#include "fusion/rasterfuse/TmeshPackgenTraverser.h"
#include "common/khGetopt.h"
#include "common/khFileUtils.h"

namespace {
const uint32 kDefaultNumCPUs = 1;

void usage(const std::string &progn, const char *msg = 0, ...) {
  if (msg) {
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
  }

  fprintf(stderr,
          "\n"
          "usage: %s [options] {--imagery|--terrain} --config <configfile>"
          " -o <output>\n"
          "   Supported options are:\n"
          "      --help | -?:     Display this usage message\n"
          "      --numcpus <num>: Num CPUs to use (default %u)\n",
          progn.c_str(), kDefaultNumCPUs);
  exit(1);
}
}  // namespace

int main(int argc, char *argv[]) {
  std::string progname = argv[0];

  // ***** process command line arguments *****
  int argn;
  bool help = false;
  std::string configFile;
  std::string output;
  uint32 numcpus = kDefaultNumCPUs;
  bool imagery = false;
  bool terrain = false;
  khGetopt options;
  options.flagOpt("help", help);
  options.flagOpt("?", help);
  options.opt("config", configFile,  khGetopt::FileExists);
  options.opt("output", output);
  options.opt("numcpus", numcpus,
              &khGetopt::RangeValidator<uint32, 1, kMaxNumJobsLimit_2>);
  options.flagOpt("imagery", imagery);
  options.flagOpt("terrain", terrain);
  if (!options.processAll(argc, argv, argn)) {
    usage(progname);
  }
  if (argn < argc) {
    usage(progname, "extra stuff on commandline");
  }
  if (help)
    usage(progname);

  numcpus = std::min(numcpus, CommandlineNumCPUsDefault());

  // Validate commandline options
  if (imagery && terrain) {
    usage(progname, "--imagery and --terrain are exclusive");
  }
  if (!imagery && !terrain) {
    usage(progname, "--imagery or --terrain is required");
  }
  if (output.empty()) {
    usage(progname, "You must specify an output file");
  }
  if (configFile.empty()) {
    usage(progname, "You must specify --config");
  }
  if (numcpus < 1) {
    usage(progname, "Number of CPUs should not be less than 1");
  }

  try {
    // load the config
    PacketLevelConfig config;
    if (!config.Load(configFile)) {
      notify(NFY_FATAL, "Unable to load config \"%s\"",
             configFile.c_str());
    }

    // Print the input file sizes for diagnostic log file info.
    // Here we want to take in the sizes of the insets from the config file.
    std::vector<std::string> input_files;
    input_files.push_back(configFile);
    for (std::vector<PacketLevelConfig::Inset>::const_iterator
           entry = config.insets.begin(); entry != config.insets.end();
         ++entry) {
      input_files.push_back(entry->dataRP);
      input_files.push_back(entry->alphaRP);
    }
    khPrintFileSizes("Input File Sizes", input_files);

    geFilePool file_pool;

    if (terrain) {
      fusion::rasterfuse::TmeshPackgenTraverser trav(config, file_pool, output);
      trav.Traverse(numcpus);
    } else {
      bool is_mercator = !config.insets.empty() &&
          (config.insets[0].dataRP.find(
              std::string(kMercatorImageryAssetSuffix) + "/") !=
           std::string::npos);
      if (is_mercator) {
        fusion::rasterfuse::MercatorImageryPackgenTraverser trav(
            config, file_pool, output);
        trav.Traverse(numcpus);
      } else {
        fusion::rasterfuse::ProtobufImageryPackgenTraverser trav(
            config, file_pool, output);
        trav.Traverse(numcpus);
      }
    }
  } catch(const std::exception &e) {
    notify(NFY_FATAL, "%s", e.what());
  } catch(...) {
    notify(NFY_FATAL, "Unknown error");
  }

  // On successful completion, print out the output file sizes.
  std::vector<std::string> output_files;
  output_files.push_back(output);
  khPrintFileSizes("Output File Sizes", output_files);

  return 0;
}
