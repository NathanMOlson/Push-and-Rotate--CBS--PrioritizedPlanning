#include "mission.h"
#include "astar.h"
#include "dijkstra.h"
#include "xmllogger.h"
#include "gl_const.h"
#include <iostream>
#include <numeric>

Mission::Mission()
{
    logger = nullptr;
    multiagentSearch = nullptr;
    mapFile = nullptr;
}

Mission::Mission (const char* MapFile)
{
    mapFile = MapFile;
    logger = nullptr;
    multiagentSearch = nullptr;
}

Mission::~Mission()
{
    if (logger)
        delete logger;
    if (multiagentSearch)
        delete multiagentSearch;
}

bool Mission::getMap()
{
    return map.getMap(mapFile);
}

bool Mission::getAgents(const char* agentsFile)
{
    agentSet.clear();
    return agentSet.readAgents(agentsFile);
}

bool Mission::getConfig()
{
    return config.getConfig(mapFile);
}

bool Mission::createLog()
{
    if (logger != NULL) delete logger;
    logger = new XmlLogger(config.LogParams[CN_LP_LEVEL]);
    return logger->getLog(mapFile, config.LogParams);
}

/*void Mission::createEnvironmentOptions()
{
    if (config.SearchParams[CN_SP_ST] == CN_SP_ST_BFS || config.SearchParams[CN_SP_ST] == CN_SP_ST_DIJK)
        options = EnvironmentOptions(config.SearchParams[CN_SP_AS], config.SearchParams[CN_SP_AD],
                                     config.SearchParams[CN_SP_CC]);
    else
        options = EnvironmentOptions(config.SearchParams[CN_SP_AS], config.SearchParams[CN_SP_AD],
                                     config.SearchParams[CN_SP_CC], config.SearchParams[CN_SP_MT]);
}*/

/*void Mission::createSearch()
{
    if (config.lowLevel == CN_SP_ST_SCIPP) {
        searchType = CN_SP_ST_SCIPP;
        //search = new SCIPP<>(config.focalW);
    } else if (config.withFocalSearch) {
        searchType = CN_SP_ST_FOCAL_SEARCH;
        //search = new FocalSearch<>(true, config.focalW);
    } else if (config.lowLevel == CN_SP_ST_ASTAR) {
        if (config.searchType == CN_ST_CBS || config.searchType == CN_ST_PP) {
            searchType = CN_SP_ST_TIME_ASTAR;
        } else {
            searchType = CN_SP_ST_ASTAR;
        }
        //search = new Astar<>(config.searchType == CN_ST_CBS || config.searchType == CN_ST_PP);
    } else {
        searchType = CN_SP_ST_SIPP;
        //search = new SIPP<>();
    }
}*/


void Mission::createAlgorithm()
{
    if (config.searchType == CN_ST_PR) {
        multiagentSearch = new PushAndRotate(new Astar<>(false));
    } else if (config.searchType == CN_ST_CBS) {
        if (config.lowLevel == CN_SP_ST_ASTAR) {
            multiagentSearch = new ConflictBasedSearch<Astar<>>(new Astar<>(true));
        } else if (config.lowLevel == CN_SP_ST_SIPP) {
            multiagentSearch = new ConflictBasedSearch<SIPP<>>(new SIPP<>());
        } else if (config.lowLevel == CN_SP_ST_WSIPP) {
            multiagentSearch = new ConflictBasedSearch<WeightedSIPP<>>(new WeightedSIPP<>(config.focalW, config.genSuboptFromOpt));
        } else if (config.lowLevel == CN_SP_ST_FS) {
            multiagentSearch = new ConflictBasedSearch<FocalSearch<>>(new FocalSearch<>(true, config.focalW));
        } else if (config.lowLevel == CN_SP_ST_SCIPP) {
            multiagentSearch = new ConflictBasedSearch<SCIPP<>>(new SCIPP<>(config.focalW));
        }
    } else if (config.searchType == CN_ST_PP) {
        if (config.lowLevel == CN_SP_ST_ASTAR) {
            multiagentSearch = new PrioritizedPlanning<Astar<>>(new Astar<>(true));
        } else if (config.lowLevel == CN_SP_ST_SIPP) {
            multiagentSearch = new PrioritizedPlanning<SIPP<>>(new SIPP<>());
        }
    }
}

bool Mission::checkAgentsCorrectness(const std::string &agentsFile) {
    if (agentSet.getAgentCount() < config.maxAgents) {
        std::cout << "Warning: not enough agents in " << agentsFile <<
                     " agents file. This file will be ignored" << std::endl;
        return false;
    }
    for (int i = 0; i < agentSet.getAgentCount(); ++i) {
        Agent agent = agentSet.getAgent(i);
        Node start = agent.getStartPosition(), goal = agent.getGoalPosition();
        if (!map.CellOnGrid(start.i, start.j) || !map.CellOnGrid(goal.i, goal.j) ||
            map.CellIsObstacle(start.i, start.j) || map.CellIsObstacle(goal.i, goal.j)) {
            std::cout << "Warning: start or goal position of agent " << agent.getId() << " in " << agentsFile <<
                         " agents file is incorrect. This file will be ignored" << std::endl;
            return false;
        }
    }
    for (int i = 0; i < agentSet.getAgentCount(); ++i) {
        for (int j = i + 1; j < agentSet.getAgentCount(); ++j) {
            if (agentSet.getAgent(i).getStartPosition() == agentSet.getAgent(j).getStartPosition()) {
                std::cout << "Warning: start positions of agents " << i << " and " << j <<
                             " in " << agentsFile << " are in the same cell. This file will be ignored" << std::endl;
                return false;
            } else if (agentSet.getAgent(i).getGoalPosition() == agentSet.getAgent(j).getGoalPosition()) {
                std::cout << "Warning: goal positions of agents " << i << " and " << j <<
                             " in " << agentsFile << " are in the same cell. This file will be ignored" << std::endl;
                return false;
            }
        }
    }
    return true;
}

void Mission::startSearch(const std::string &agentsFile)
{
    int minAgents = config.singleExecution ? config.maxAgents : config.minAgents;
    for (int i = minAgents; i <= config.maxAgents; ++i) {
        AgentSet curAgentSet;
        for (int j = 0; j < i; ++j) {
            Agent agent = agentSet.getAgent(j);
            curAgentSet.addAgent(agent.getCur_i(), agent.getCur_j(), agent.getGoal_i(), agent.getGoal_j());
        }

        multiagentSearch->clear();
        sr = multiagentSearch->startSearch(map, config, curAgentSet);
        if (!sr.pathfound) {
            std::cout << "Failed to find solution for " << i << " agents" << std::endl;
            if (config.singleExecution) {
                std::cout << "Log will not be created" << std::endl;
            }
            break;
        }

        agentsPaths = *(sr.agentsPaths);
        std::pair<int, int> costs = getCosts();
        int makespan = costs.first;
        int flowtime = costs.second;
        makespans[i].push_back(makespan);
        flowtimes[i].push_back(flowtime);
        times[i].push_back(sr.time);

        if (config.singleExecution) {
            saveAgentsPathsToLog(agentsFile, sr.time, makespan, flowtime);
        }
        if (!checkCorrectness()) {
            std::cout << "Search returned incorrect results!" << std::endl;
            break;
        }
        std::cout << "Found solution for " << i << " agents. Time: " <<
                    sr.time << ", flowtime: " << flowtime << ", makespan: " << makespan << std::endl;
    }
}

std::pair<int, int> Mission::getCosts() {
    size_t makespan = 0, timeflow = 0;
    for (int i = 0; i < agentsPaths.size(); ++i) {
        makespan = std::max(makespan, agentsPaths[i].size() - 1);
        int lastMove;
        for (lastMove = agentsPaths[i].size() - 1; lastMove > 1 && agentsPaths[i][lastMove] == agentsPaths[i][lastMove - 1]; --lastMove);
        timeflow += lastMove;
    }
    return std::make_pair(makespan, timeflow);
}

bool Mission::checkCorrectness() {
    size_t agentCount = agentsPaths.size();
    size_t solutionSize = 0;
    for (int j = 0; j < agentCount; ++j) {
        solutionSize = std::max(solutionSize, agentsPaths[j].size());
    }
    std::vector<std::vector<Node>::iterator> starts, ends;
    for (int j = 0; j < agentCount; ++j) {
        if (agentsPaths[j][0] != agentSet.getAgent(j).getStartPosition()) {
            std::cout << "Incorrect result: agent path starts in wrong position!" << std::endl;
            return false;
        }
        if (agentsPaths[j].back() != agentSet.getAgent(j).getGoalPosition()) {
            std::cout << "Incorrect result: agent path ends in wrong position!" << std::endl;
            return false;
        }
        starts.push_back(agentsPaths[j].begin());
        ends.push_back(agentsPaths[j].end());
    }

    for (int i = 0; i < solutionSize; ++i) {
        for (int j = 0; j < agentCount; ++j) {
            if (i >= agentsPaths[j].size()) {
                continue;
            }
            if (map.CellIsObstacle(agentsPaths[j][i].i, agentsPaths[j][i].j)) {
                std::cout << "Incorrect result: agent path goes through obstacle!" << std::endl;
                return false;
            }
            if (i > 0 &&
                abs(agentsPaths[j][i].i - agentsPaths[j][i - 1].i) +
                abs(agentsPaths[j][i].j - agentsPaths[j][i - 1].j) > 1) {
                std::cout << "Incorrect result: consecutive nodes in agent path are not adjacent!" << std::endl;
                return false;
            }
        }
    }
    ConflictSet conflictSet = ConflictBasedSearch<>::findConflict<std::vector<Node>::iterator>(starts, ends);
    if (!conflictSet.empty()) {
        Conflict conflict = conflictSet.getBestConflict();
        if (conflict.edgeConflict) {
            std::cout << "Incorrect result: two agents swap positions!" << std::endl;
        } else {
            std::cout << "Incorrect result: two agents occupy the same node!" << std::endl;
        }
        return false;
    }
    return true;
}

void Mission::saveAggregatedResultsToLog() {
    std::map<int, int> successCount;
    std::map<int, double> makespansAggregated, timeflowsAggregated, timesAggregated;
    for (int i = config.minAgents; i <= config.maxAgents; ++i) {
        if (times[i].size() > 0) {
            successCount[i] = times[i].size();
            makespansAggregated[i] = std::accumulate(makespans[i].begin(), makespans[i].end(), 0.0) / makespans[i].size();
            timeflowsAggregated[i] = std::accumulate(flowtimes[i].begin(), flowtimes[i].end(), 0.0) / flowtimes[i].size();
            timesAggregated[i] = std::accumulate(times[i].begin(), times[i].end(), 0.0) / times[i].size();
        }
    }
    logger->writeToLogAggregatedResults(successCount, makespansAggregated, timeflowsAggregated, timesAggregated);
    logger->saveLog();
}

/*void Mission::printSearchResultsToConsole()
{
    std::cout << "Path ";
    if (!sr.pathfound)
        std::cout << "NOT ";
    std::cout << "found!" << std::endl;
    std::cout << "numberofsteps=" << sr.numberofsteps << std::endl;
    std::cout << "nodescreated=" << sr.nodescreated << std::endl;
    if (sr.pathfound) {
        std::cout << "pathlength=" << sr.pathlength << std::endl;
        std::cout << "pathlength_scaled=" << sr.pathlength * map.getCellSize() << std::endl;
    }
    std::cout << "time=" << sr.time << std::endl;
}*/

void Mission::saveAgentsPathsToLog(const std::string &agentsFile, double time, int makespan, int flowtime)
{
    logger->writeToLogAgentsPaths(agentSet, agentsPaths, agentsFile, time, makespan, flowtime);
    logger->saveLog();
}

int Mission::getTasksCount() {
    return config.tasksCount;
}

std::string Mission::getAgentsFile() {
    return config.agentsFile;
}

bool Mission::getSingleExecution() {
    return config.singleExecution;
}
