// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "cld2_dynamic_data.h"
#include "cld2_dynamic_data_extractor.h"
#include "cld2_dynamic_data_loader.h"
#include "integral_types.h"
#include "cld2tablesummary.h"
#include "utf8statetable.h"
#include "scoreonescriptspan.h"

// We need these in order to set up a real data object to pass around.                              
namespace CLD2 {
  extern const UTF8PropObj cld_generated_CjkUni_obj;
  extern const CLD2TableSummary kCjkCompat_obj;
  extern const CLD2TableSummary kCjkDeltaBi_obj;
  extern const CLD2TableSummary kDistinctBiTable_obj;
  extern const CLD2TableSummary kQuad_obj;
  extern const CLD2TableSummary kQuad_obj2;
  extern const CLD2TableSummary kDeltaOcta_obj;
  extern const CLD2TableSummary kDistinctOcta_obj;
  extern const short kAvgDeltaOctaScore[];
}

int main(int argc, char** argv) {
  if (!CLD2DynamicData::isLittleEndian()) {
    std::cerr << "System is big-endian: currently not supported." << std::endl;
    return -1;
  }
  if (!CLD2DynamicData::coreAssumptionsOk()) {
    std::cerr << "Core assumptions violated, unsafe to continue." << std::endl;
    return -2;
  }

  // Get command-line flags
  int flags = 0;
  bool get_vector = false;
  char* fileName = NULL;
  const char* USAGE = "\
CLD2 Dynamic Data Tool:\n\
Dump, verify or print summaries of scoring tables for CLD2.\n\
\n\
The files output by this tool are suitable for all little-endian platforms,\n\
and should work on both 32- and 64-bit platforms.\n\
\n\
IMPORTANT: The files output by this tool WILL NOT work on big-endian platforms.\n\
\n\
Usage:\n\
  --dump [FILE]     Dump the scoring tables that this tool was linked against\n\
                    to the specified file. The tables are automatically verified\n\
                    after writing, just as if the tool was run again with\n\
                    '--verify'.\n\
  --verify [FILE]   Verify that a given file precisely matches the scoring\n\
                    tables that this tool was linked against. This can be used\n\
                    to verify that a file is compatible.\n\
  --head [FILE]     Print headers from the specified file to stdout.\n\
  --verbose         Be verbose.\n\
";
  int mode = 0; //1=dump, 2=verify, 3=head
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--verbose") == 0) {
      CLD2DynamicDataExtractor::setDebug(1);
      CLD2DynamicData::setDebug(1);
    }
    else if (strcmp(argv[i], "--dump") == 0
              || strcmp(argv[i], "--verify") == 0
              || strcmp(argv[i], "--head") == 0) {

      // set mode flag properly
      if (strcmp(argv[i], "--dump") == 0) mode=1;
      else if (strcmp(argv[i], "--verify") == 0) mode=2;
      else mode=3;
      if (i < argc - 1) {
        fileName = argv[++i];
      } else {
        std::cerr << "missing file name argument" << std::endl << std::endl;
        std::cerr << USAGE;
        return -1;
      }
    } else if (strcmp(argv[i], "--help") == 0) {
      std::cout << USAGE;
      return 0;
    } else {
      std::cerr << "Unsupported option: " << argv[i] << std::endl << std::endl;
      std::cerr << USAGE;
      return -1;
    }
  }

  if (mode == 0) {
    std::cerr << USAGE;
    return -1;
  }

  CLD2::ScoringTables realData = {
    &CLD2::cld_generated_CjkUni_obj,
    &CLD2::kCjkCompat_obj,
    &CLD2::kCjkDeltaBi_obj,
    &CLD2::kDistinctBiTable_obj,
    &CLD2::kQuad_obj,
    &CLD2::kQuad_obj2,
    &CLD2::kDeltaOcta_obj,
    &CLD2::kDistinctOcta_obj,
    CLD2::kAvgDeltaOctaScore,
  };
  if (mode == 1) { // dump
    CLD2DynamicDataExtractor::writeDataFile(
      static_cast<const CLD2::ScoringTables*>(&realData),
      fileName);
  } else if (mode == 3) { // head
    CLD2DynamicData::FileHeader* header = CLD2DynamicDataLoader::loadHeader(fileName);
    if (header == NULL) {
      std::cerr << "Cannot read header from file: " << fileName << std::endl;
      return -1;
    }
    CLD2DynamicData::dumpHeader(header);
    delete header->tableHeaders;
    delete header;
  }
  
  if (mode == 1 || mode == 2) { // dump || verify (so perform verification)
    void* mmapAddress = NULL;
    int mmapLength = 0;
    CLD2::ScoringTables* loadedData = CLD2DynamicDataLoader::loadDataFile(fileName, &mmapAddress, &mmapLength);

    if (loadedData == NULL) {
      std::cerr << "Failed to read data file: " << fileName << std::endl;
      return -1;
    }
    bool result = CLD2DynamicData::verify(
      static_cast<const CLD2::ScoringTables*>(&realData),
      static_cast<const CLD2::ScoringTables*>(loadedData));
    CLD2DynamicDataLoader::unloadData(&loadedData, &mmapAddress, &mmapLength);
    if (loadedData != NULL || mmapAddress != NULL || mmapLength != 0) {
      std::cerr << "Warning: failed to clean up memory for ScoringTables." << std::endl;
    }
    if (!result) {
      std::cerr << "Verification failed!" << std::endl;
      return -1;
    }
  }
}
