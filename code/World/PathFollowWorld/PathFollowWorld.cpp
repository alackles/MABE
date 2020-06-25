//  MABE is a product of The Hintze Lab @ MSU
//     for general research information:
//         hintzelab.msu.edu
//     for MABE documentation:
//         github.com/Hintzelab/MABE/wiki
//
//  Copyright (c) 2015 Michigan State University. All rights reserved.
//     to view the full license, visit:
//         github.com/Hintzelab/MABE/wiki/License

#include "PathFollowWorld.h"

// this is how you setup a parameter in MABE, the function Parameters::register_parameter()takes the
// name of the parameter (catagory-name), default value (which must conform with the type), a the useage message
shared_ptr<ParameterLink<int>> PathFollowWorld::evaluationsPerGenerationPL =
    Parameters::register_parameter("WORLD_PATHFOLLOW-evaluationsPerGeneration", 3,
    "how many times should each organism be tested in each generation?");

shared_ptr<ParameterLink<std::string>> PathFollowWorld::mapNamesPL =
Parameters::register_parameter("WORLD_PATHFOLLOW-mapNames",
    (std::string)"../code/World/pathFollowWorld/path1.txt,../code/World/pathFollowWorld/path2.txt",
    "list of text files with paths. in path files, 0 = empty, 1 = forward path, 2 = turn right, 3 = turn right, 4 = end of path");

shared_ptr<ParameterLink<int>> PathFollowWorld::randomizeTurnSignsPL =
Parameters::register_parameter("WORLD_PATHFOLLOW-randomizeTurnSigns", 1,
    "will turn signs be randomized?\n"
    "if 0, no\n"
    "if 1, values 2 and 3 may be swaped\n"
    "if 2, values all 2s will be replace by a number in range [5,9] and all 3's by a different number in range [5,9]");

shared_ptr<ParameterLink<int>> PathFollowWorld::stepsPL =
Parameters::register_parameter("WORLD_PATHFOLLOW-steps", 100,
    "how many many steps can the agent take on each map?");

shared_ptr<ParameterLink<bool>> PathFollowWorld::binaryInputsPL =
Parameters::register_parameter("WORLD_PATHFOLLOW-binaryInputs", true,
    "if true, inputs will be converted to binary");

shared_ptr<ParameterLink<double>> PathFollowWorld::emptySpaceCostPL =
Parameters::register_parameter("WORLD_PATHFOLLOW-emptySpaceCost", .25,
    "score lost anytime agent is on an empty location (including non-empty locations that become empty)");


// load single line from file, lines that are empty or start with # are skipped
inline bool loadLineFromFile(std::ifstream& file, std::string& rawLine, std::stringstream& ss) {
    rawLine.clear();
    if (file.is_open() && !file.eof()) {
        while ((rawLine.size() == 0 || rawLine[0] == '#') && !file.eof()) {
            getline(file, rawLine);
            // remove all whitespace from rawLine
            rawLine.erase(remove_if(rawLine.begin(), rawLine.end(), ::isspace), rawLine.end());
        }
        ss.str(std::string());
ss << rawLine;
    }
    else if (!file.eof()) {
    std::cout << "in loadSS, file is not open!\n  Exiting." << std::endl;
    exit(1);
    }
    //std::cout << "from file:  " << rawLine << std::endl;
    return file.eof();
}


void PathFollowWorld::loadMaps(std::vector<string>& mapNames, std::vector<Vector2d<int>>& maps, std::vector<std::pair<int, int>>& mapSizes, std::vector<std::pair<int, int>>& startLocations) {
    for (auto mapName : mapNames) {

        std::string rawLine;
        std::stringstream ss;

        std::ifstream FILE(mapName);
        bool atEOF = loadLineFromFile(FILE, rawLine, ss);
        std::vector<int> tempVect;
        int tempInt;

        convertCSVListToVector(rawLine, tempVect);

        mapSizes.push_back({ tempVect[0], tempVect[1] });
        maps.push_back(Vector2d<int>(tempVect[0], tempVect[1]));

        atEOF = loadLineFromFile(FILE, rawLine, ss);

        size_t y = 0;
        while (!atEOF) {
            for (size_t x = 0; x < rawLine.size(); x++) {
                if (rawLine[x] == 'X') { // this is the start location, put 'path' here
                    maps.back()((int)x, (int)y) = 1;
                    startLocations.push_back({ x,y });
                }
                else {
                    maps.back()((int)x, (int)y) = rawLine[x] - 48;
                    //convertString(std::to_string(rawLine[x]), maps.back()((int)x, (int)y));// tempInt);
                }
            }
            y += 1;
            atEOF = loadLineFromFile(FILE, rawLine, ss);
        }
    }
}



// the constructor gets called once when MABE starts up. use this to set things up
PathFollowWorld::PathFollowWorld(shared_ptr<ParametersTable> PT_) : AbstractWorld(PT_) {

    //localize a parameter value for faster access
    evaluationsPerGeneration = evaluationsPerGenerationPL->get(PT);
    convertCSVListToVector(mapNamesPL->get(PT), mapNames);
    randomizeTurnSigns = randomizeTurnSignsPL->get(PT);
    steps = stepsPL->get(PT);
    binaryInputs = binaryInputsPL->get(PT);
    emptySpaceCost = emptySpaceCostPL->get(PT);

    std::cout << "In pathFollowWorld, loading maps:" << std::endl;
    loadMaps(mapNames, maps, mapSizes, startLocations);


    for (size_t i = 0; i < maps.size(); i++) {

        maxScores.push_back(0);
        forwardCounts.push_back(0);
        int minSteps = 0;
        for (int y = 0; y < mapSizes[i].second; y++) {
            for (int x = 0; x < mapSizes[i].first; x++) {
                if (maps[i](x, y) > 0) { // if location is not empty
                    if (maps[i](x, y) != 4) {
                        maxScores[i]++;
                        minSteps += (maps[i](x, y) > 1) ? 2 : 1; // if location is 1 (forward) add one, if > 1 (turn) add 2
                    }
                }
            }
        }
        forwardCounts[i] = maxScores[i];
        maxScores[i] += (steps - minSteps);

        std::cout << "\n" << mapNames[i] << "  " << startLocations[i].first << "," << startLocations[i].second << std::endl;
        std::cout << "min steps: " << minSteps << "  maxScore: " << maxScores[i] << "  forwardCounts: " << forwardCounts[i] << std::endl;
        maps[i].showGrid();
    }

    //exit(1);

    // popFileColumns tell MABE what data should be saved to pop.csv files
    popFileColumns.clear();
    popFileColumns.push_back("score");
    popFileColumns.push_back("reachGoal");
    popFileColumns.push_back("completion");
}

// the evaluate function gets called every generation. evaluate should set values on organisms datamaps
// that will be used by other parts of MABE for things like reproduction and archiving
auto PathFollowWorld::evaluate(map<string, shared_ptr<Group>>& groups, int analyze, int visualize, int debug) -> void {

    int popSize = groups[groupName]->population.size();

    // in this world, organisms donot interact, so we can just iterate over the population
    for (int i = 0; i < popSize; i++) {

        // create a shortcut to access the organism and organisms brain
        auto org = groups[groupName]->population[i];
        auto brain = org->brains[brainName];
        
        int xPos, yPos, direction, out0, out1;
        double score, reachGoal;

        // evaluate this organism some number of times based on evaluationsPerGeneration
        for (int t = 0; t < evaluationsPerGeneration; t++) {
            // clear the brain - resets brain state including memory
            for (size_t i = 0; i < maps.size(); i++) {
                // new map! reset score and reachGoal
                score = 0;
                reachGoal = 0;

                // set starting location using value from file for this map
                xPos = startLocations[i].first;
                yPos = startLocations[i].second;
                direction = 0;
                int thisForwardCount = 0;

                // make a copy of the map so we can change it
                auto thisMap = maps[i];

                // if randomizeTurnSigns is 1 then we may swap lefts and rights in the map
                if (randomizeTurnSigns == 1) {
                    if (t%2) { // switch every other eval
                        // look at all locations in map and swap turn sign values
                        for (int y = 0; y < mapSizes[i].second; y++) {
                            for (int x = 0; x < mapSizes[i].first; x++) {
                                if (thisMap(x, y) == 2) {
                                    thisMap(x, y) = 3;
                                }
                                else if (thisMap(x, y) == 3) {
                                    thisMap(x, y) = 2;
                                }
                            }
                        }
                    }
                }
                // if randomizeTurnsigns == 2 then we will randomize lefts and rights to values in [5,9]
                else if (randomizeTurnSigns == 2) {
                    int newSign2 = Random::getInt(5, 9);
                    int newSign3 = Random::getInt(5, 8); // one less, and then add one if the same as newSign2
                    if (newSign3 == newSign2) {
                        ++newSign3;
                    }
                    // look at all locations in map and substitute turn sign values
                    for (int y = 0; y < mapSizes[i].second; y++) {
                        for (int x = 0; x < mapSizes[i].first; x++) {
                            if (thisMap(x, y) == 2) {
                                thisMap(x, y) = newSign2;
                            }
                            else if (thisMap(x, y) == 3) {
                                thisMap(x, y) = newSign3;
                            }
                        }
                    }
                }

                if (debug) {
                    // show grid (after possible turn sign randomization and agent location
                    thisMap.showGrid();
                    std::cout << "at location: " << xPos << "," << yPos << "  direction: " << direction << std::endl;
                }

                brain->resetBrain(); // before starting evaluation on this map

                int step; // agents will have "steps" time to solve the path
                for (step = 0; step < steps; step++) {
                    
                    if (visualize) {
                        // show grid, and other stats
                        for (int y = 0; y < mapSizes[i].second; y++) {
                            for (int x = 0; x < mapSizes[i].first; x++) {
                                if (x == xPos && y == yPos) {
                                    std::cout << "* ";
                                }
                                else if (thisMap(x, y) == 0) {
                                    std::cout << "  ";
                                }
                                else if (thisMap(x,y) < 0) {
                                    std::cout << thisMap(x, y);
                                }
                                else {
                                    std::cout << thisMap(x, y) << " ";
                                }
                            }
                            std::cout << std::endl;
                        }

                        std::cout << "at location: " << xPos << "," << yPos << "  direction: " << direction << std::endl;
                        std::cout << "forward steps taken: " << thisForwardCount << "  current score: " << score << std::endl;
                        std::cout << "value @ this location: " << thisMap(xPos, yPos) << std::endl;
                    }

                    // first set input(s)
                    if (binaryInputs) {
                        // if binary inputs, all values passed to the brain will be 1 or 0
                        if (randomizeTurnSigns < 2) {
                            brain->setInput(0, thisMap(xPos, yPos) == 0); // is location empty
                            brain->setInput(1, thisMap(xPos, yPos) == 1); // is location path
                            brain->setInput(2, thisMap(xPos, yPos) == 2); // is location turn 0
                            brain->setInput(3, thisMap(xPos, yPos) == 3); // is location turn 1
                            brain->setInput(4, thisMap(xPos, yPos) == 4); // is location goal
                        }
                        else {
                            brain->setInput(0, thisMap(xPos, yPos) == 0); // is location empty
                            brain->setInput(1, thisMap(xPos, yPos) == 1); // is location path
                            std::vector<int> turnSignal(4,0);
                            if (thisMap(xPos, yPos) > 4) { // if > 4 it this is a turn
                                int c = 0;
                                int val = thisMap(xPos, yPos);
                                // convert the value at location to bits
                                while (val > 0){
                                    brain->setInput(2 + c, val & 1);
                                    if (debug) {
                                        std::cout << val << "(bit " << c << ") = " << (val & 1) << std::endl;
                                    }
                                    val = val>>1;
                                    ++c;
                                }
                            }
                            else { // value is <= 4, then set the inputs for "turn signal" all to 0
                                brain->setInput(2, thisMap(xPos, yPos) == 4); // is location goal
                                brain->setInput(3, thisMap(xPos, yPos) == 4); // is location goal
                                brain->setInput(4, thisMap(xPos, yPos) == 4); // is location goal
                                brain->setInput(5, thisMap(xPos, yPos) == 4); // is location goal
                            }
                            brain->setInput(6, thisMap(xPos, yPos) == 4); // is location goal

                        }
                    }
                    else { // not binaryInputs, only one input, the value at the current location
                        brain->setInput(0, thisMap(xPos, yPos));
                    }
                    // next score based on map value at this location, and update map
                    // if map location = 1, +1 score, and change map location value to 0
                    // if map location = 4, goal. add any remaning steps to score, and set step = steps (so while loop will end)
                    // if map location > 1, set location value to 1 (this is a turn sign), the value will be 1 next update if this agent turns, which will provide +1 score
                    if (thisMap(xPos, yPos) > 1) {
                        if (thisMap(xPos, yPos) == 4) { // if we get to the goal, get extra points for time left
                            //std::cout << "TFC: " << thisForwardCount << "  " << thisForwardCount + 5 << " " << forwardCounts[i] << std::endl;
                            if (thisForwardCount >= forwardCounts[i]) { // if all forward locations have been visited...
                                reachGoal = 1;
                                score += steps - step; // add points for time left
                            }
                            step = steps;
                        }
                        thisMap(xPos, yPos) = 1;// -1 * thisMap(xPos, yPos);
                    }
                    else if (thisMap(xPos, yPos) == 1) {
                        thisMap(xPos, yPos) = 0;
                        score += 1;
                        thisForwardCount += 1;
                    }
                    else {
                        // if current location is empty, pay emptySpaceCost
                        score -= emptySpaceCost;
                    }

                    brain->update();

                    out0 = Bit(brain->readOutput(0));
                    out1 = Bit(brain->readOutput(1));
                    if (visualize) {
                        std::cout << "outputs: " << out0 << "," << out1 << std::endl;
                    }

                    if (out0 == 1 && out1 == 1) { // forward
                        xPos = std::max(0, std::min(xPos + dx[direction], mapSizes[i].first-1));
                        yPos = std::max(0, std::min(yPos + dy[direction], mapSizes[i].second-1));
                    }
                    else if (out0 == 0 && out1 == 0) { // reverse
                        xPos = std::max(0, std::min(xPos - dx[direction], mapSizes[i].first-1));
                        yPos = std::max(0, std::min(yPos - dy[direction], mapSizes[i].second-1));
                    }
                    else if (out0 == 1 && out1 == 0) { // left
                        direction = loopMod(direction - 1, 8);
                    }
                    else if (out0 == 0 && out1 == 1) { // right
                        direction = loopMod(direction + 1, 8);
                    }
                }
                org->dataMap.append("completion", (double)thisForwardCount / (double)forwardCounts[i]);
                org->dataMap.append("score", score / maxScores[i]);
                org->dataMap.append("reachGoal", reachGoal);
                if (debug || visualize) {
                    std::cout << "completion: " << (double)thisForwardCount / (double)forwardCounts[i] << std::endl;
                    std::cout << "score: " << score / maxScores[i] << std::endl;
                    std::cout << "reachGoal: " << reachGoal << std::endl;;
                }
            }
        }
    } // end of population loop
}

// the requiredGroups function lets MABE know how to set up populations of organisms that this world needs
auto PathFollowWorld::requiredGroups() -> unordered_map<string,unordered_set<string>> {
    if (binaryInputs) {
        if (randomizeTurnSigns == 2) {
            std::cout << "pathFollowWorld requires B:" + brainName + ",7,2" << std::endl;
            return { { groupName, { "B:" + brainName + ",7,2" } } }; // inputs: empty, path, turn bits 0 -> 4; outputs: left, righ
        }
        else {
            std::cout << "pathFollowWorld requires B:" + brainName + ",5,2" << std::endl;
            return { { groupName, { "B:" + brainName + ",5,2" } } }; // inputs: empty, path, turn sign 0, turn signal 1; outputs: left, right
        }
    }
    else {
        std::cout << "pathFollowWorld requires B:" + brainName + ",1,2" << std::endl;
        return { { groupName, { "B:" + brainName + ",1,2" } } }; // inputs: map value; outputs: left, right  
    }
        // this tells MABE to make a group called "root::" with a brain called "root::" that takes 2 inputs and has 1 output
        // "root::" here also indicates the namespace for the parameters used to define these elements.
        // "root::" is the default namespace, so parameters defined without a namespace are "root::" parameters
}
