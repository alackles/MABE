//  MABE is a product of The Hintze Lab @ MSU
//     for general research information:
//         hintzelab.msu.edu
//     for MABE documentation:
//         github.com/Hintzelab/MABE/wiki
//
//  Copyright (c) 2015 Michigan State University. All rights reserved.
//     to view the full license, visit:
//         github.com/Hintzelab/MABE/wiki/License

#include "LODwAPArchivist.h"

std::shared_ptr<ParameterLink<std::string>> LODwAPArchivist::LODwAP_Arch_dataSequencePL =
    Parameters::register_parameter(
        "ARCHIVIST_LODWAP-dataSequence", (std::string) ":100",
        "How often to write to data file. (format: x = single value, x-y = x "
        "to y, x-y:z = x to y on x, :z = from 0 to updates on z, x:z = from x "
        "to 'updates' on z) e.g. '1-100:10, 200, 300:100'");
std::shared_ptr<ParameterLink<std::string>>
    LODwAPArchivist::LODwAP_Arch_organismSequencePL =
        Parameters::register_parameter(
            "ARCHIVIST_LODWAP-organismsSequence", (std::string) ":1000",
            "How often to write genome file. (format: x = single value, x-y = "
            "x to y, x-y:z = x to y on x, :z = from 0 to updates on z, x:z = "
            "from x to 'updates' on z) e.g. '1-100:10, 200, 300:100'");
std::shared_ptr<ParameterLink<int>> LODwAPArchivist::LODwAP_Arch_pruneIntervalPL =
    Parameters::register_parameter(
        "ARCHIVIST_LODWAP-pruneInterval", 100,
        "How often to attempt to prune LOD and actually write out to files");
std::shared_ptr<ParameterLink<int>> LODwAPArchivist::LODwAP_Arch_terminateAfterPL =
    Parameters::register_parameter(
        "ARCHIVIST_LODWAP-terminateAfter", 10,
        "how long to run after updates (to get allow time for coalescence)");
std::shared_ptr<ParameterLink<bool>> LODwAPArchivist::LODwAP_Arch_writeDataFilePL =
    Parameters::register_parameter("ARCHIVIST_LODWAP-writeDataFile", true,
                                   "if true, a data file will be written");
std::shared_ptr<ParameterLink<bool>>
    LODwAPArchivist::LODwAP_Arch_writeOrganismFilePL =
        Parameters::register_parameter(
            "ARCHIVIST_LODWAP-writeOrganismsFile", true,
            "if true, an organisms file will be written");
std::shared_ptr<ParameterLink<std::string>> LODwAPArchivist::LODwAP_Arch_FilePrefixPL =
    Parameters::register_parameter("ARCHIVIST_LODWAP-filePrefix",
                                   (std::string) "NONE", "prefix for files saved by "
                                                    "this archivst. \"NONE\" "
                                                    "indicates no prefix.");

LODwAPArchivist::LODwAPArchivist(std::vector<std::string> popFileColumns,
                                 std::shared_ptr<Abstract_MTree> _maxFormula,
                                 std::shared_ptr<ParametersTable> PT_,
                                 std::string group_prefix)
    : DefaultArchivist(popFileColumns, _maxFormula, PT_, group_prefix) {

  pruneInterval = LODwAP_Arch_pruneIntervalPL->get(PT);
  terminateAfter = LODwAP_Arch_terminateAfterPL->get(PT);
  DataFileName = ((LODwAP_Arch_FilePrefixPL->get(PT) == "NONE")
                      ? ""
                      : LODwAP_Arch_FilePrefixPL->get(PT)) +
                 (((group_prefix_ == "")
                       ? "LOD_data.csv"
                       : group_prefix_.substr(0, group_prefix_.size() - 2) +
                             "__" + "LOD_data.csv"));
  OrganismFileName = ((LODwAP_Arch_FilePrefixPL->get(PT) == "NONE")
                          ? ""
                          : LODwAP_Arch_FilePrefixPL->get(PT)) +
                     (((group_prefix_ == "")
                           ? "LOD_organisms.csv"
                           : group_prefix_.substr(0, group_prefix_.size() - 2) +
                                 "__" + "LOD_organisms.csv"));

  writeDataFile = LODwAP_Arch_writeDataFilePL->get(PT);
  writeOrganismFile = LODwAP_Arch_writeOrganismFilePL->get(PT);

  dataSequence.push_back(0);
  organismSequence.push_back(0);

  auto dataSequenceStr = LODwAP_Arch_dataSequencePL->get(PT);
  auto genomeSequenceStr = LODwAP_Arch_organismSequencePL->get(PT);

  dataSequence = seq(dataSequenceStr, Global::updatesPL->get(), true);
  if (dataSequence.size() == 0) {
    std::cout << "unable to translate ARCHIVIST_LODWAP-dataSequence \""
         << dataSequenceStr << "\".\nExiting." << std::endl;
    exit(1);
  }
  organismSequence = seq(genomeSequenceStr, Global::updatesPL->get(), true);
  if (organismSequence.size() == 0) {
    std::cout << "unable to translate ARCHIVIST_LODWAP-organismsSequence \""
         << genomeSequenceStr << "\".\nExiting." << std::endl;
    exit(1);
  }

  if (writeDataFile != false) {
    dataSequence.clear();
    dataSequence = seq(dataSequenceStr, Global::updatesPL->get(), true);
    if (dataSequence.size() == 0) {
      std::cout << "unable to translate ARCHIVIST_SSWD-dataSequence \""
           << dataSequenceStr << "\".\nExiting." << std::endl;
      exit(1);
    }
  }

  if (writeOrganismFile != false) {
    organismSequence = seq(genomeSequenceStr, Global::updatesPL->get(), true);
    if (organismSequence.size() == 0) {
      std::cout << "unable to translate ARCHIVIST_SSWD-organismsSequence \""
           << genomeSequenceStr << "\".\nExiting." << std::endl;
      exit(1);
    }
  }

  dataSeqIndex = 0;
  organismSeqIndex = 0;
  nextDataWrite = dataSequence[dataSeqIndex];
  nextOrganismWrite = organismSequence[organismSeqIndex];

  lastPrune = 0;
}

bool LODwAPArchivist::archive(std::vector<std::shared_ptr<Organism>> &population,
                              int flush) {
  if (finished_ && !flush) {
    return finished_;
  }

  if ((Global::update == realtimeSequence[realtime_sequence_index_]) &&
      (flush == 0)) { // do not write files on flush - these organisms have not
                      // been evaluated!
    writeRealTimeFiles(population); // write to Max and average files
    if (realtime_sequence_index_ + 1 < (int)realtimeSequence.size()) {
      realtime_sequence_index_++;
    }
  }

  if ((Global::update == realtimeDataSequence[realtime_data_seq_index_]) &&
      (flush == 0) &&
      writeSnapshotDataFiles) { // do not write files on flush - these organisms
                                // have not been evaluated!
    saveSnapshotData(population);
    if (realtime_data_seq_index_ + 1 < (int)realtimeDataSequence.size()) {
      realtime_data_seq_index_++;
    }
  }
  if ((Global::update ==
       realtimeOrganismSequence[realtime_organism_seq_index_]) &&
      (flush == 0) &&
      writeSnapshotGenomeFiles) { // do not write files on flush - these
                                  // organisms have not been evaluated!
    saveSnapshotOrganisms(population);
    if (realtime_organism_seq_index_ + 1 <
        (int)realtimeOrganismSequence.size()) {
      realtime_organism_seq_index_++;
    }
  }

  if (writeOrganismFile &&
      find(organismSequence.begin(), organismSequence.end(), Global::update) !=
          organismSequence.end()) {
    for (auto org : population) { // if this update is in the genome sequence,
                                  // turn on genome tracking.
      org->trackOrganism = true;
    }
  }

  if ((Global::update % pruneInterval == 0) || (flush == 1)) {

    if (files_.find(DataFileName) ==
        files_.end()) { // if file has not be initialized yet
      files_[DataFileName].push_back("update");
      files_[DataFileName].push_back("timeToCoalescence");
      for (auto key :
           population[0]->dataMap.getKeys()) { // store keys from data map
                                               // associated with file name
        files_[DataFileName].push_back(key);
      }
    }

    // get the MRCA
    std::vector<std::shared_ptr<Organism>> LOD =
        population[0]->getLOD(population[0]); // get line of decent
    std::shared_ptr<Organism> effective_MRCA;
    std::shared_ptr<Organism> real_MRCA;
    if (flush) { // if flush then we don't care about coalescence
      std::cout << "flushing LODwAP: using population[0] as Most Recent Common "
              "Ancestor (MRCA)"
           << std::endl;
      effective_MRCA = population[0]->parents[0]; // this assumes that a
                                                  // population was created, but
                                                  // not tested at the end of
                                                  // the evolution loop!
      real_MRCA = population[0]->getMostRecentCommonAncestor(
          LOD); // find the convergance point in the LOD.
    } else {
      effective_MRCA = population[0]->getMostRecentCommonAncestor(
          LOD); // find the convergance point in the LOD.
      real_MRCA = effective_MRCA;
    }

    // Save Data
    int TTC;
    if (writeDataFile) {
      while ((effective_MRCA->timeOfBirth >= nextDataWrite) &&
             (nextDataWrite <=
              Global::updatesPL->get())) { // if there is convergence before the
                                           // next data interval
        std::shared_ptr<Organism> current = LOD[nextDataWrite - lastPrune];
        current->dataMap.set("update", nextDataWrite);
        current->dataMap.setOutputBehavior("update", DataMap::FIRST);
        TTC = std::max(0, current->timeOfBirth - real_MRCA->timeOfBirth);
        current->dataMap.set("timeToCoalescence", TTC);
        current->dataMap.setOutputBehavior("timeToCoalescence", DataMap::FIRST);
        current->dataMap.writeToFile(
            DataFileName, files_[DataFileName]); // append new data to the file
        current->dataMap.clear("update");
        current->dataMap.clear("timeToCoalescence");
        if ((int)dataSequence.size() > dataSeqIndex + 1) {
          dataSeqIndex++;
          nextDataWrite = dataSequence[dataSeqIndex];
        } else {
          nextDataWrite = Global::updatesPL->get() + terminateAfter + 1;
        }
      }
      if (flush) {
        std::cout << "Most Recent Common Ancestor/Time to Coalescence was " << TTC
             << " updates ago." << std::endl;
      }
    }

    // Save Organisms
    if (writeOrganismFile) {

      while ((effective_MRCA->timeOfBirth >= nextOrganismWrite) &&
             (nextOrganismWrite <=
              Global::updatesPL->get())) { // if there is convergence before the
                                           // next data interval

        std::shared_ptr<Organism> current = LOD[nextOrganismWrite - lastPrune];

        DataMap OrgMap;
        OrgMap.set("ID", current->ID);
        OrgMap.set("update", nextOrganismWrite);
        OrgMap.setOutputBehavior("update", DataMap::FIRST);
        std::string tempName;

        for (auto genome : current->genomes) {
          tempName = "GENOME_" + genome.first;
          OrgMap.merge(genome.second->serialize(tempName));
        }
        for (auto brain : current->brains) {
          tempName = "BRAIN_" + brain.first;
          OrgMap.merge(brain.second->serialize(tempName));
        }
        OrgMap.writeToFile(OrganismFileName); // append new data to the file

        if ((int)organismSequence.size() > organismSeqIndex + 1) {
          organismSeqIndex++;
          nextOrganismWrite = organismSequence[organismSeqIndex];
        } else {
          nextOrganismWrite = Global::updatesPL->get() + terminateAfter + 1;
        }
      }
    }
    // data and genomes have now been written out up till the MRCA
    // so all data and genomes from before the MRCA can be deleted
    effective_MRCA->parents.clear();
    lastPrune = effective_MRCA->timeOfBirth; // this will hold the time of the
                                             // oldest genome in RAM
  }

  // if we have reached the end of time OR we have pruned past updates (i.e.
  // written out all data up to updates), then we ae done!
  // cout << "\nHERE" << endl;
  // cout << Global::update << " >= " << Global::updatesPL->get() << " + " <<
  // terminateAfter << endl;
  finished_ = finished_ ||
              (Global::update >= Global::updatesPL->get() + terminateAfter ||
               lastPrune >= Global::updatesPL->get());

  /*
  ////////////////////////////////////////////////////////
  // code to support defualt archivist snapshotData
  ////////////////////////////////////////////////////////
  vector<shared_ptr<Organism>> toCheck;
  unordered_set<shared_ptr<Organism>> checked;
  int minBirthTime = population[0]->timeOfBirth; // time of birth of oldest org
  being saved in this update (init with random value)

  for (auto org : population) {  // we don't need to worry about tracking
  parents or lineage, so we clear out this data every generation.
          if (!writeSnapshotDataFiles) {
                  org->parents.clear();
          }
          else if (org->snapshotAncestors.find(org->ID) !=
  org->snapshotAncestors.end()) { // if ancestors contains self, then this org
  has been saved and it's ancestor list has been collapsed
                  org->parents.clear();
                  checked.insert(org); // make a note, so we don't check this
  org later
                  minBirthTime = min(org->timeOfBirth, minBirthTime);
          }
          else { // org has not ever been saved to file...
                  toCheck.push_back(org); // we will need to check to see if we
  can do clean up related to this org
                  checked.insert(org); // make a note, so we don't check twice
                  minBirthTime = min(org->timeOfBirth, minBirthTime);
          }
  }
  ////////////////////////////////////////////////////////
  // end code to support defualt archivist snapshotData
  ////////////////////////////////////////////////////////
  */

  return finished_;
}

